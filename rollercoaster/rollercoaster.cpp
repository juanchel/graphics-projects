#include "stdafx.h"
#include "RgbImage.h"
#include <pic.h>
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <iostream>
#include <vector>
#include <GL/glu.h>
#include <GL/glut.h>

/* represents one control point along the spline */
struct point {
	double x;
	double y;
	double z;
};

// saves information about the track so it doesnt need to be calculated
struct trackPoint 
{
	point point;
	double normal[3];
	double tangent[3];
	double binormal[3];
	double rail[12][3];
	double eye[3];
	double lookat[3];
};

/* spline struct which contains how many control points, and an array of control points */
struct spline {
	int numControlPoints;
	struct point *points;
};

std::vector<trackPoint> g_Track;

/* the spline array */
struct spline *g_Splines;

/* total number of splines */
int g_iNumOfSplines;

RgbImage g_texGrass;
RgbImage g_texSkybox;

bool g_bMove = false;
bool g_bTime = true;
int g_iCurrentPoint = 0;
int g_iCurrentFrac = 0;
int g_iSpeed = 20;
int g_iTimeOfDay = 0;
int g_iScreenshotNumber = 0;


char filename[8] = "000.jpg";

float g_Terrain[65][65];
float g_TerrainNormal[64][64][6];
float g_fLightIntensity = 1;
float g_fLightColor[] = {1, 1, 1};

int loadSplines(char *argv) {
	char *cName = (char *)malloc(128 * sizeof(char));
	FILE *fileList;
	FILE *fileSpline;
	int iType, i = 0, j, iLength;

	/* load the track file */
	fileList = fopen(argv, "r");
	if (fileList == NULL) {
		printf ("can't open file\n");
		exit(1);
	}

	/* stores the number of splines in a global variable */
	fscanf(fileList, "%d", &g_iNumOfSplines);

	g_Splines = (struct spline *)malloc(g_iNumOfSplines * sizeof(struct spline));


	/* reads through the spline files */
	for (j = 0; j < g_iNumOfSplines; j++) {
		i = 0;
		fscanf(fileList, "%s", cName);
		fileSpline = fopen(cName, "r");

		if (fileSpline == NULL) {
			printf ("can't open file\n");
			exit(1);
		}

		/* gets length for spline file */
		fscanf(fileSpline, "%d %d", &iLength, &iType);

		/* allocate memory for all the points */
		g_Splines[j].points = (struct point *)malloc(iLength * sizeof(struct point));
		g_Splines[j].numControlPoints = iLength;

		/* saves the data to the struct */
		while (fscanf(fileSpline, "%lf %lf %lf", 
			&g_Splines[j].points[i].x, 
			&g_Splines[j].points[i].y, 
			&g_Splines[j].points[i].z) != EOF) {
				i++;
		}
	}

	free(cName);

	return 0;
}

// load a texture from bmp
void loadTextureFromFile(char *filename)
{    
	glClearColor (0.0, 0.0, 0.0, 0.0);
	glShadeModel(GL_FLAT);
	glEnable(GL_DEPTH_TEST);

	RgbImage theTexMap(filename);

	gluBuild2DMipmaps(GL_TEXTURE_2D, 3,theTexMap.GetNumCols(), theTexMap.GetNumRows(),
		GL_RGB, GL_UNSIGNED_BYTE, theTexMap.ImageData());
}

/* Write a screenshot to the specified filename */
void saveScreenshot (char *filename)
{
	int i, j;
	Pic *in = NULL;

	if (filename == NULL)
		return;

	/* Allocate a picture buffer */
	in = pic_alloc(640, 480, 3, NULL);

	printf("File to save to: %s\n", filename);

	for (i=479; i>=0; i--) {
		glReadPixels(0, 479-i, 640, 1, GL_RGB, GL_UNSIGNED_BYTE,
			&in->pix[i*in->nx*in->bpp]);
	}

	if (jpeg_write(filename, in))
		printf("File saved Successfully\n");
	else
		printf("Error in Saving\n");

	pic_free(in);
}

/* returns the name of the next jpg to use */
char* nameJPG()
{
	char number[4];

	itoa(g_iScreenshotNumber, number, 10);

	if (g_iScreenshotNumber < 10)
		filename[2] = number[0];
	else if (g_iScreenshotNumber < 100)
	{
		filename[1] = number[0];
		filename[2] = number[1];
	}
	else
	{
		filename[0] = number[0];
		filename[1] = number[1];
		filename[2] = number[2];
	}

	g_iScreenshotNumber++;
	return filename;
}


// calculate where something from a catmull rom spline should be
point catmullRom(point p1, point p2, point p3, point p4, double t)
{
	point pt;

	pt.x = 0.5 * ( (-1*p1.x + 3*p2.x - 3*p3.x + p4.x) * t * t * t
		+ (2*p1.x - 5*p2.x + 4*p3.x - p4.x) * t * t
		+ (-1*p1.x + p3.x) * t
		+ 2*p2.x);

	pt.y = 0.5 * ( (-1*p1.y + 3*p2.y - 3*p3.y + p4.y) * t * t * t
		+ (2*p1.y - 5*p2.y + 4*p3.y - p4.y) * t * t
		+ (-1*p1.y + p3.y) * t
		+ 2*p2.y);

	pt.z = 0.5 * ( (-1*p1.z + 3*p2.z - 3*p3.z + p4.z) * t * t * t
		+ (2*p1.z - 5*p2.z + 4*p3.z - p4.z) * t * t
		+ (-1*p1.z + p3.z) * t
		+ 2*p2.z);

	return pt;
}

// idle function
void doIdle()
{
	// change the time of day and light
	if (g_bTime)
	{
		if (g_iTimeOfDay < 1000)
		{
			g_iTimeOfDay+=1;

			g_fLightIntensity = min(1,max(abs(float(g_iTimeOfDay-500))-250,25)/125);

			if (g_iTimeOfDay<125 || g_iTimeOfDay>875)
			{		
				g_fLightColor[0] = 1;
				g_fLightColor[1] = 1;
				g_fLightColor[2] = 1;
			}
			else if (g_iTimeOfDay>375 && g_iTimeOfDay<625)
			{
				g_fLightColor[0] = .2;
				g_fLightColor[1] = .2;
				g_fLightColor[2] = .2;
			}
			else if (g_iTimeOfDay < 500)
			{
				double mult = ((float)g_iTimeOfDay-125)/250;
				g_fLightColor[0] = 1-mult*.8;
				g_fLightColor[1] = max(.2, 1-2*mult);
				g_fLightColor[2] = max(.2, 1-2*mult);
			}
			else
			{
				double mult = ((float)g_iTimeOfDay-625)/250;
				g_fLightColor[0] = 0.2+mult*.8;
				g_fLightColor[1] = max(.2, -1+2*mult);
				g_fLightColor[2] = max(.2, -1+2*mult);
			}

		}
		else
		{
			g_iTimeOfDay = 0;
		}
	}

	// calculate the light depending on the time of day
	GLfloat ambientLight[] = {0.2*g_fLightIntensity*g_fLightColor[0], 0.2*g_fLightIntensity*g_fLightColor[1], 0.2*g_fLightIntensity*g_fLightColor[2], 1.0};
	GLfloat diffuseLight[] = {0.8*g_fLightIntensity*g_fLightColor[0], 0.8*g_fLightIntensity*g_fLightColor[1], 0.8*g_fLightIntensity*g_fLightColor[2], 1.0};
	GLfloat specularLight[] = {g_fLightIntensity*g_fLightColor[0], g_fLightIntensity*g_fLightColor[1], g_fLightIntensity*g_fLightColor[2], 1.0};
	GLfloat lightPosition[] = {0.0, cos((float)g_iTimeOfDay*.00628), sin((float)g_iTimeOfDay*.00628), 0.0};
	glLightfv(GL_LIGHT0, GL_AMBIENT, ambientLight);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuseLight);
	glLightfv(GL_LIGHT0, GL_SPECULAR, specularLight);
	glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);

	// move the camera position depending on speed
	if (g_bMove) {

		if (g_iCurrentPoint < g_Track.size()-30)
		{
			g_iCurrentFrac+=g_iSpeed/2;
			while (g_iCurrentFrac > 32)
			{
				g_iCurrentPoint+=g_iCurrentFrac/32;
				g_iCurrentFrac%=32;
			}
		}

		if (g_Track[g_iCurrentPoint].point.y < g_Track[g_iCurrentPoint+1].point.y)
		{
			g_iSpeed-=(g_Track[g_iCurrentPoint+1].point.y-g_Track[g_iCurrentPoint].point.y)*10;
			if (g_iSpeed<16)
				g_iSpeed=16;
		}

		if (g_Track[g_iCurrentPoint].point.y > g_Track[g_iCurrentPoint+1].point.y && g_iSpeed < 96)
		{
			g_iSpeed+=(g_Track[g_iCurrentPoint].point.y-g_Track[g_iCurrentPoint+1].point.y)*20;
		}
	}
	
	//saveScreenshot(nameJPG());
	glutPostRedisplay();
}

// precalculation of the spline data
void calcSplines()
{
	for (int i=0; i<g_Splines[0].numControlPoints-2; i++)
	{
		point p1 = g_Splines[0].points[i];
		point p2 = g_Splines[0].points[i+1];
		point p3 = g_Splines[0].points[i+2];
		point p4 = g_Splines[0].points[i+3];

		for (double t = 0.0; t<1; t+=.04)
		{
			point pt = catmullRom(p1, p2, p3, p4, t);
			trackPoint tpt;
			tpt.point.x = pt.x;
			tpt.point.y = pt.z;
			tpt.point.z = pt.y;
			g_Track.push_back(tpt);
		}

		// calculate tangent based on derivative
		double lastPoint[] = {g_Splines[0].points[0].x, g_Splines[0].points[0].y, g_Splines[0].points[0].z};

		for (int j=0; j<g_Track.size(); j++)
		{
			double tangent[] = {0, 0, 0};
			tangent[0] = g_Track[j].point.x - lastPoint[0];
			tangent[1] = g_Track[j].point.y - lastPoint[1];
			tangent[2] = g_Track[j].point.z - lastPoint[2];

			double len = sqrt(tangent[0]*tangent[0]+tangent[1]*tangent[1]+tangent[2]*tangent[2]);

			g_Track[j].tangent[0]=tangent[0]/len;
			g_Track[j].tangent[1]=tangent[1]/len;
			g_Track[j].tangent[2]=tangent[2]/len;

			lastPoint[0] = g_Track[j].point.x;
			lastPoint[1] = g_Track[j].point.y;
			lastPoint[2] = g_Track[j].point.z;
		}

		g_Track[0].tangent[0] = g_Track[1].tangent[0];
		g_Track[0].tangent[1] = g_Track[1].tangent[1];
		g_Track[0].tangent[2] = g_Track[1].tangent[2];

		// calculate binormal against straight up, calculate normal based on tangent and binormal
		double binormal[] = {0, 0, 0};
		double normal[] = {0, 0, 0};

		for (int j=0; j<g_Track.size(); j++)
		{
			binormal[0] = -1*g_Track[j].tangent[2];
			binormal[2] = g_Track[j].tangent[0];
			g_Track[j].binormal[0] = binormal[0];
			g_Track[j].binormal[1] = binormal[1];
			g_Track[j].binormal[2] = binormal[2];

			normal[0] = g_Track[j].tangent[1]*g_Track[j].binormal[2] - g_Track[j].tangent[2]*g_Track[j].binormal[1];
			normal[1] = g_Track[j].tangent[2]*g_Track[j].binormal[0] - g_Track[j].tangent[0]*g_Track[j].binormal[2];
			normal[2] = g_Track[j].tangent[0]*g_Track[j].binormal[1] - g_Track[j].tangent[1]*g_Track[j].binormal[0];
			g_Track[j].normal[0] = -1*normal[0];
			g_Track[j].normal[1] = -1*normal[1];
			g_Track[j].normal[2] = -1*normal[2];
		}

		// incremental spline calculations, unused

		/*
		for (int j=1; j<g_Track.size(); j++)
		{

		normal[0] = g_Track[j].tangent[1]*g_Track[j-1].binormal[2] - g_Track[j].tangent[2]*g_Track[j-1].binormal[1];
		normal[1] = g_Track[j].tangent[2]*g_Track[j-1].binormal[0] - g_Track[j].tangent[0]*g_Track[j-1].binormal[2];
		normal[2] = g_Track[j].tangent[0]*g_Track[j-1].binormal[1] - g_Track[j].tangent[1]*g_Track[j-1].binormal[0];
		g_Track[j].normal[0] = normal[0];
		g_Track[j].normal[1] = normal[1];
		g_Track[j].normal[2] = normal[2];

		binormal[0] = g_Track[j].tangent[2]*g_Track[j-1].normal[1] - g_Track[j].tangent[1]*g_Track[j-1].normal[2];
		binormal[1] = g_Track[j].tangent[0]*g_Track[j-1].normal[2] - g_Track[j].tangent[2]*g_Track[j-1].normal[0];
		binormal[2] = g_Track[j].tangent[1]*g_Track[j-1].normal[0] - g_Track[j].tangent[0]*g_Track[j-1].normal[1];
		g_Track[j].binormal[0] = binormal[0];
		g_Track[j].binormal[1] = binormal[1];
		g_Track[j].binormal[2] = binormal[2];
		}
		*/


		// normalize calculated vectors
		for (int j=0; j<g_Track.size(); j++)
		{

			double len = 1;
			len  = sqrt(g_Track[j].normal[0]*g_Track[j].normal[0]+
				g_Track[j].normal[1]*g_Track[j].normal[1] + g_Track[j].normal[2]*g_Track[j].normal[2]);

			g_Track[j].normal[0]=g_Track[j].normal[0]/len;
			g_Track[j].normal[1]=g_Track[j].normal[1]/len;
			g_Track[j].normal[2]=g_Track[j].normal[2]/len;

			len  = sqrt(g_Track[j].binormal[0]*g_Track[j].binormal[0]+
				g_Track[j].binormal[1]*g_Track[j].binormal[1] + g_Track[j].binormal[2]*g_Track[j].binormal[2]);

			g_Track[j].binormal[0]=g_Track[j].binormal[0]/len;
			g_Track[j].binormal[1]=g_Track[j].binormal[1]/len;
			g_Track[j].binormal[2]=g_Track[j].binormal[2]/len;
		}


		// Save the position of the rails
		// Doing this means that they do not need to be calculated constantly
		// 1  2               6  5
		//  8  9               10 11
		// 0  3               7  4
		for (int j=0; j<g_Track.size(); j++)
		{
			g_Track[j].rail[0][0] = g_Track[j].point.x;
			g_Track[j].rail[0][1] = g_Track[j].point.y;
			g_Track[j].rail[0][2] = g_Track[j].point.z;

			g_Track[j].rail[1][0] = g_Track[j].rail[0][0] + g_Track[j].normal[0]*.05;
			g_Track[j].rail[1][1] = g_Track[j].rail[0][1] + g_Track[j].normal[1]*.05;
			g_Track[j].rail[1][2] = g_Track[j].rail[0][2] + g_Track[j].normal[2]*.05;

			g_Track[j].rail[2][0] = g_Track[j].rail[1][0] + g_Track[j].binormal[0]*.05;
			g_Track[j].rail[2][1] = g_Track[j].rail[1][1] + g_Track[j].binormal[1]*.05;
			g_Track[j].rail[2][2] = g_Track[j].rail[1][2] + g_Track[j].binormal[2]*.05;

			g_Track[j].rail[3][0] = g_Track[j].rail[2][0] - g_Track[j].normal[0]*.05;
			g_Track[j].rail[3][1] = g_Track[j].rail[2][1] - g_Track[j].normal[1]*.05;
			g_Track[j].rail[3][2] = g_Track[j].rail[2][2] - g_Track[j].normal[2]*.05;

			g_Track[j].rail[4][0] = g_Track[j].point.x + g_Track[j].binormal[0];
			g_Track[j].rail[4][1] = g_Track[j].point.y + g_Track[j].binormal[1];
			g_Track[j].rail[4][2] = g_Track[j].point.z + g_Track[j].binormal[2];

			g_Track[j].rail[5][0] = g_Track[j].rail[4][0] + g_Track[j].normal[0]*.05;
			g_Track[j].rail[5][1] = g_Track[j].rail[4][1] + g_Track[j].normal[1]*.05;
			g_Track[j].rail[5][2] = g_Track[j].rail[4][2] + g_Track[j].normal[2]*.05;

			g_Track[j].rail[6][0] = g_Track[j].rail[5][0] - g_Track[j].binormal[0]*.05;
			g_Track[j].rail[6][1] = g_Track[j].rail[5][1] - g_Track[j].binormal[1]*.05;
			g_Track[j].rail[6][2] = g_Track[j].rail[5][2] - g_Track[j].binormal[2]*.05;

			g_Track[j].rail[7][0] = g_Track[j].rail[6][0] - g_Track[j].normal[0]*.05;
			g_Track[j].rail[7][1] = g_Track[j].rail[6][1] - g_Track[j].normal[1]*.05;
			g_Track[j].rail[7][2] = g_Track[j].rail[6][2] - g_Track[j].normal[2]*.05;

			g_Track[j].rail[8][0] = g_Track[j].rail[0][0] + g_Track[j].tangent[0]*.05;
			g_Track[j].rail[8][1] = g_Track[j].rail[0][1] + g_Track[j].tangent[1]*.05;
			g_Track[j].rail[8][2] = g_Track[j].rail[0][2] + g_Track[j].tangent[2]*.05;

			g_Track[j].rail[9][0] = g_Track[j].rail[3][0] + g_Track[j].tangent[0]*.05;
			g_Track[j].rail[9][1] = g_Track[j].rail[3][1] + g_Track[j].tangent[1]*.05;
			g_Track[j].rail[9][2] = g_Track[j].rail[3][2] + g_Track[j].tangent[2]*.05;

			g_Track[j].rail[10][0] = g_Track[j].rail[7][0] + g_Track[j].tangent[0]*.05;
			g_Track[j].rail[10][1] = g_Track[j].rail[7][1] + g_Track[j].tangent[1]*.05;
			g_Track[j].rail[10][2] = g_Track[j].rail[7][2] + g_Track[j].tangent[2]*.05;

			g_Track[j].rail[11][0] = g_Track[j].rail[4][0] + g_Track[j].tangent[0]*.05;
			g_Track[j].rail[11][1] = g_Track[j].rail[4][1] + g_Track[j].tangent[1]*.05;
			g_Track[j].rail[11][2] = g_Track[j].rail[4][2] + g_Track[j].tangent[2]*.05;

			g_Track[j].eye[0]=g_Track[j].point.x+g_Track[j].binormal[0]/2+g_Track[j].normal[0];
			g_Track[j].eye[1]=g_Track[j].point.y+g_Track[j].binormal[1]/2+g_Track[j].normal[1];
			g_Track[j].eye[2]=g_Track[j].point.z+g_Track[j].binormal[2]/2+g_Track[j].normal[2];

			g_Track[j].lookat[0]=g_Track[j].tangent[0]+g_Track[j].point.x+g_Track[j].binormal[0]/2+g_Track[j].normal[0];
			g_Track[j].lookat[1]=g_Track[j].tangent[1]+g_Track[j].point.y+g_Track[j].binormal[1]/2+g_Track[j].normal[1];
			g_Track[j].lookat[2]=g_Track[j].tangent[2]+g_Track[j].point.z+g_Track[j].binormal[2]/2+g_Track[j].normal[2];

		}
	}
}

// draw the track
void drawTrack() 
{
	// set a steelish metaly color
	GLfloat materialAmbient[] = {0.5, 0.5, 0.7, 1.0};
	GLfloat materialDiffuse[] = {0.5, 0.5, 0.7, 1.0};
	GLfloat materialSpecular[] = {1.0, 1.0, 1.0, 1.0};

	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, materialAmbient);
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, materialDiffuse);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, materialSpecular);
	glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 128.0);

	glBegin(GL_TRIANGLE_STRIP);
	for (int i=0; i<g_Track.size()-30; i++)
	{
		glNormal3f(-1*g_Track[i].binormal[0],-1*g_Track[i].binormal[1],-1*g_Track[i].binormal[2]);
		glVertex3f(g_Track[i].rail[0][0],g_Track[i].rail[0][1],g_Track[i].rail[0][2]);
		glVertex3f(g_Track[i].rail[1][0],g_Track[i].rail[1][1],g_Track[i].rail[1][2]);
	}
	glEnd();

	glBegin(GL_TRIANGLE_STRIP);
	for (int i=0; i<g_Track.size()-30; i++)
	{
		glNormal3f(g_Track[i].normal[0],g_Track[i].normal[1],g_Track[i].normal[2]);
		glVertex3f(g_Track[i].rail[1][0],g_Track[i].rail[1][1],g_Track[i].rail[1][2]);
		glVertex3f(g_Track[i].rail[2][0],g_Track[i].rail[2][1],g_Track[i].rail[2][2]);
	}
	glEnd();

	glBegin(GL_TRIANGLE_STRIP);
	for (int i=0; i<g_Track.size()-30; i++)
	{
		glNormal3f(g_Track[i].binormal[0],g_Track[i].binormal[1],g_Track[i].binormal[2]);
		glVertex3f(g_Track[i].rail[2][0],g_Track[i].rail[2][1],g_Track[i].rail[2][2]);
		glVertex3f(g_Track[i].rail[3][0],g_Track[i].rail[3][1],g_Track[i].rail[3][2]);
	}
	glEnd();

	glBegin(GL_TRIANGLE_STRIP);
	for (int i=0; i<g_Track.size()-30; i++)
	{
		glNormal3f(-1*g_Track[i].normal[0],-1*g_Track[i].normal[1],-1*g_Track[i].normal[2]);
		glVertex3f(g_Track[i].rail[3][0],g_Track[i].rail[3][1],g_Track[i].rail[3][2]);
		glVertex3f(g_Track[i].rail[0][0],g_Track[i].rail[0][1],g_Track[i].rail[0][2]);
	}
	glEnd();

	glBegin(GL_TRIANGLE_STRIP);
	for (int i=0; i<g_Track.size()-30; i++)
	{
		glNormal3f(g_Track[i].binormal[0],g_Track[i].binormal[1],g_Track[i].binormal[2]);
		glVertex3f(g_Track[i].rail[4][0],g_Track[i].rail[4][1],g_Track[i].rail[4][2]);
		glVertex3f(g_Track[i].rail[5][0],g_Track[i].rail[5][1],g_Track[i].rail[5][2]);
	}
	glEnd();

	glBegin(GL_TRIANGLE_STRIP);
	for (int i=0; i<g_Track.size()-30; i++)
	{
		glNormal3f(g_Track[i].normal[0],g_Track[i].normal[1],g_Track[i].normal[2]);
		glVertex3f(g_Track[i].rail[5][0],g_Track[i].rail[5][1],g_Track[i].rail[5][2]);
		glVertex3f(g_Track[i].rail[6][0],g_Track[i].rail[6][1],g_Track[i].rail[6][2]);
	}
	glEnd();

	glBegin(GL_TRIANGLE_STRIP);
	for (int i=0; i<g_Track.size()-30; i++)
	{
		glNormal3f(-1*g_Track[i].binormal[0],-1*g_Track[i].binormal[1],-1*g_Track[i].binormal[2]);
		glVertex3f(g_Track[i].rail[6][0],g_Track[i].rail[6][1],g_Track[i].rail[6][2]);
		glVertex3f(g_Track[i].rail[7][0],g_Track[i].rail[7][1],g_Track[i].rail[7][2]);
	}
	glEnd();

	glBegin(GL_TRIANGLE_STRIP);
	for (int i=0; i<g_Track.size()-30; i++)
	{
		glNormal3f(-1*g_Track[i].normal[0],-1*g_Track[i].normal[1],-1*g_Track[i].normal[2]);
		glVertex3f(g_Track[i].rail[7][0],g_Track[i].rail[7][1],g_Track[i].rail[7][2]);
		glVertex3f(g_Track[i].rail[4][0],g_Track[i].rail[4][1],g_Track[i].rail[4][2]);
	}
	glEnd();

	// draw the supports
	for (int i=2; i<g_Track.size()-30; i++)
	{
		if(i%16 == 0)
		{
			glBegin(GL_QUAD_STRIP);
			glVertex3f(g_Track[i].rail[0][0],g_Track[i].rail[0][1],g_Track[i].rail[0][2]);
			glVertex3f(g_Track[i].rail[0][0],-100,g_Track[i].rail[0][2]);

			glNormal3f(-1*g_Track[i].tangent[0],-1*g_Track[i].tangent[0],-1*g_Track[i].tangent[0]);
			glVertex3f(g_Track[i].rail[3][0],g_Track[i].rail[3][1],g_Track[i].rail[3][2]);
			glVertex3f(g_Track[i].rail[3][0],-100,g_Track[i].rail[3][2]);

			glNormal3f(g_Track[i].binormal[0],g_Track[i].binormal[0],g_Track[i].binormal[0]);
			glVertex3f(g_Track[i].rail[9][0],g_Track[i].rail[9][1],g_Track[i].rail[9][2]);
			glVertex3f(g_Track[i].rail[9][0],-100,g_Track[i].rail[9][2]);

			glNormal3f(g_Track[i].tangent[0],g_Track[i].tangent[0],g_Track[i].tangent[0]);
			glVertex3f(g_Track[i].rail[8][0],g_Track[i].rail[8][1],g_Track[i].rail[8][2]);
			glVertex3f(g_Track[i].rail[8][0],-100,g_Track[i].rail[8][2]);

			glNormal3f(-1*g_Track[i].binormal[0],-1*g_Track[i].binormal[0],-1*g_Track[i].binormal[0]);
			glVertex3f(g_Track[i].rail[0][0],g_Track[i].rail[0][1],g_Track[i].rail[0][2]);
			glVertex3f(g_Track[i].rail[0][0],-100,g_Track[i].rail[0][2]);
			glEnd();

			glBegin(GL_QUAD_STRIP);
			glVertex3f(g_Track[i].rail[7][0],g_Track[i].rail[7][1],g_Track[i].rail[7][2]);
			glVertex3f(g_Track[i].rail[7][0],-100,g_Track[i].rail[7][2]);

			glNormal3f(-1*g_Track[i].tangent[0],-1*g_Track[i].tangent[0],-1*g_Track[i].tangent[0]);
			glVertex3f(g_Track[i].rail[4][0],g_Track[i].rail[4][1],g_Track[i].rail[4][2]);
			glVertex3f(g_Track[i].rail[4][0],-100,g_Track[i].rail[4][2]);

			glNormal3f(g_Track[i].binormal[0],g_Track[i].binormal[0],g_Track[i].binormal[0]);
			glVertex3f(g_Track[i].rail[11][0],g_Track[i].rail[11][1],g_Track[i].rail[11][2]);
			glVertex3f(g_Track[i].rail[11][0],-100,g_Track[i].rail[11][2]);

			glNormal3f(g_Track[i].tangent[0],g_Track[i].tangent[0],g_Track[i].tangent[0]);
			glVertex3f(g_Track[i].rail[10][0],g_Track[i].rail[10][1],g_Track[i].rail[10][2]);
			glVertex3f(g_Track[i].rail[10][0],-100,g_Track[i].rail[10][2]);

			glNormal3f(-1*g_Track[i].binormal[0],-1*g_Track[i].binormal[0],-1*g_Track[i].binormal[0]);
			glVertex3f(g_Track[i].rail[7][0],g_Track[i].rail[7][1],g_Track[i].rail[7][2]);
			glVertex3f(g_Track[i].rail[7][0],-100,g_Track[i].rail[7][2]);
			glEnd();
		}
	}

	// draw the planks
	// get a brown woodlike color
	GLfloat materialAmbient2[] = {0.54, 0.27, 0.07, 1.0};
	GLfloat materialDiffuse2[] = {0.54, 0.27, 0.07, 1.0};
	GLfloat materialSpecular2[] = {0.1, 0.1, 0.1, 1.0};

	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, materialAmbient2);
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, materialDiffuse2);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, materialSpecular2);
	glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 32.0);

	glBegin(GL_QUADS);
	for (int i=2; i<g_Track.size()-30; i++)
	{
		if(i%4 == 0)
		{
			glNormal3f(g_Track[i].binormal[0],g_Track[i].binormal[1],g_Track[i].binormal[2]);
			glVertex3f(g_Track[i].rail[2][0],g_Track[i].rail[2][1],g_Track[i].rail[2][2]);
			glVertex3f(g_Track[i].rail[3][0],g_Track[i].rail[3][1],g_Track[i].rail[3][2]);
			glVertex3f(g_Track[i].rail[7][0],g_Track[i].rail[7][1],g_Track[i].rail[7][2]);
			glVertex3f(g_Track[i].rail[6][0],g_Track[i].rail[6][1],g_Track[i].rail[6][2]);

			glNormal3f(-1*g_Track[i].binormal[0],-1*g_Track[i].binormal[1],-1*g_Track[i].binormal[2]);
			glVertex3f(g_Track[i-1].rail[2][0],g_Track[i-1].rail[2][1],g_Track[i-1].rail[2][2]);
			glVertex3f(g_Track[i-1].rail[3][0],g_Track[i-1].rail[3][1],g_Track[i-1].rail[3][2]);
			glVertex3f(g_Track[i-1].rail[7][0],g_Track[i-1].rail[7][1],g_Track[i-1].rail[7][2]);
			glVertex3f(g_Track[i-1].rail[6][0],g_Track[i-1].rail[6][1],g_Track[i-1].rail[6][2]);
		}
	}
	glEnd();

	for (int i=2; i<g_Track.size()-30; i++)
	{
		if(i%4 == 0)
		{
			glBegin(GL_TRIANGLE_STRIP);
			glNormal3f(g_Track[i].normal[0],g_Track[i].normal[1],g_Track[i].normal[2]);
			glVertex3f(g_Track[i].rail[2][0],g_Track[i].rail[2][1],g_Track[i].rail[2][2]);
			glVertex3f(g_Track[i-1].rail[2][0],g_Track[i-1].rail[2][1],g_Track[i-1].rail[2][2]);
			glVertex3f(g_Track[i].rail[6][0],g_Track[i].rail[6][1],g_Track[i].rail[6][2]);
			glVertex3f(g_Track[i-1].rail[6][0],g_Track[i-1].rail[6][1],g_Track[i-1].rail[6][2]);
			glEnd();

			glBegin(GL_TRIANGLE_STRIP);
			glNormal3f(-1*g_Track[i].tangent[0],-1*g_Track[i].tangent[1],-1*g_Track[i].tangent[2]);
			glVertex3f(g_Track[i].rail[3][0],g_Track[i].rail[3][1],g_Track[i].rail[3][2]);
			glVertex3f(g_Track[i-1].rail[3][0],g_Track[i-1].rail[3][1],g_Track[i-1].rail[3][2]);
			glVertex3f(g_Track[i].rail[7][0],g_Track[i].rail[7][1],g_Track[i].rail[7][2]);
			glVertex3f(g_Track[i-1].rail[7][0],g_Track[i-1].rail[7][1],g_Track[i-1].rail[7][2]);

			glEnd();
		}
	}
}
void drawGround()
{
	//glDisable(GL_LIGHTING);
	glEnable(GL_TEXTURE_2D);
	gluBuild2DMipmaps(GL_TEXTURE_2D, 3, 512, 512, GL_RGB, GL_UNSIGNED_BYTE, g_texGrass.ImageData());

	GLfloat materialAmbient2[] = {1, 1, 1, 1.0};
	GLfloat materialDiffuse2[] = {1, 1, 1, 1.0};
	GLfloat materialSpecular2[] = {1, 1, 1, 1.0};

	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, materialAmbient2);
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, materialDiffuse2);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, materialSpecular2);
	glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 256.0);

	// draw the ground
	for (int j=0; j<64; j++)
	{
		float a1, a2, a3, b1, b2, b3;
		glBegin(GL_TRIANGLE_STRIP);
		for (int i=0; i<65; i++)
		{
			if (i!=0)
				glNormal3f(-1*g_TerrainNormal[i-1][j][0],-1*g_TerrainNormal[i-1][j][1],-1*g_TerrainNormal[i-1][j][2]);
			glTexCoord2f((float)i/8,(float)j/8);
			glVertex3f(-128+i*4, g_Terrain[i][j], -128+j*4);

			if (i!=0)
				glNormal3f(g_TerrainNormal[i-1][j][3],g_TerrainNormal[i-1][j][4],g_TerrainNormal[i-1][j][5]);
			glTexCoord2f((float)i/8,((float)j+1)/8);
			glVertex3f(-128+i*4, g_Terrain[i][j+1], -124+j*4);
		}
		glEnd();
	}

	glDisable(GL_TEXTURE_2D);
	//glEnable(GL_LIGHTING);

	// draw the ocean
	GLfloat materialAmbient3[] = {0.3, 0.3, 1, 1.0};
	GLfloat materialDiffuse3[] = {0.3, 0.3, 1, 1.0};
	GLfloat materialSpecular3[] = {1, 1, 1, 1.0};

	glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, materialAmbient3);
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, materialDiffuse3);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, materialSpecular3);
	glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 256.0);

	glBegin(GL_QUADS);
	glVertex3f(-500, -15.1, -500);
	glVertex3f(500, -15.1, -500);
	glVertex3f(500, -15.1, 500);
	glVertex3f(-500, -15.1, 500);
	glEnd();
}

// draw the skybox
void drawSky()
{

	glDisable(GL_LIGHTING);
	glEnable(GL_TEXTURE_2D);
	gluBuild2DMipmaps(GL_TEXTURE_2D, 3, 512, 256, GL_RGB, GL_UNSIGNED_BYTE, g_texSkybox.ImageData());

	glColor3f(g_fLightColor[0], g_fLightColor[1], g_fLightColor[2]);
	glBegin(GL_QUADS);

	GLfloat lowx = -100+g_Track[g_iCurrentPoint].eye[0];
	GLfloat lowz = -100+g_Track[g_iCurrentPoint].eye[2];
	GLfloat highx = 100+g_Track[g_iCurrentPoint].eye[0];
	GLfloat highz = 100+g_Track[g_iCurrentPoint].eye[2];

	glTexCoord2f(0.0, 0.0);
	glVertex3f(lowx, -100, lowz);
	glTexCoord2f(0.25, 0.0);
	glVertex3f(lowx, -100, highz);
	glTexCoord2f(0.25, 0.5);
	glVertex3f(lowx, 100, highz);
	glTexCoord2f(0.0, 0.5);
	glVertex3f(lowx, 100, lowz);

	glTexCoord2f(0.25, 0.0);
	glVertex3f(lowx, -100, highz);
	glTexCoord2f(0.5, 0.0);
	glVertex3f(highx, -100, highz);
	glTexCoord2f(0.5, 0.5);
	glVertex3f(highx, 100, highz);
	glTexCoord2f(0.25, 0.5);
	glVertex3f(lowx, 100, highz);

	glTexCoord2f(0.5, 0.0);
	glVertex3f(highx, -100, highz);
	glTexCoord2f(0.75, 0.0);
	glVertex3f(highx, -100, lowz);
	glTexCoord2f(0.75, 0.5);
	glVertex3f(highx, 100, lowz);
	glTexCoord2f(0.5, 0.5);
	glVertex3f(highx, 100, highz);

	glTexCoord2f(0.75, 0.0);
	glVertex3f(highx, -100, lowz);
	glTexCoord2f(1.0, 0.0);
	glVertex3f(lowx, -100, lowz);
	glTexCoord2f(1.0, 0.5);
	glVertex3f(lowx, 100, lowz);
	glTexCoord2f(0.75, 0.5);
	glVertex3f(highx, 100, lowz);

	glTexCoord2f(0.25, 1.0);
	glVertex3f(lowx, 100, lowz);
	glTexCoord2f(0.25, 0.5);
	glVertex3f(lowx, 100, highz);
	glTexCoord2f(0.5, 0.5);
	glVertex3f(highx, 100, highz);
	glTexCoord2f(0.5, 1.0);
	glVertex3f(highx, 100, lowz);

	glEnd();

	glDisable(GL_TEXTURE_2D);
	glEnable(GL_LIGHTING);
}

void display()
{

	glNormal3f(0, 1, 0);
	glClearColor( 0, 0, 0, 0 );
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glLoadIdentity();

	// change the camera by the current point and the fraction towards the next
	gluLookAt(g_Track[g_iCurrentPoint].eye[0]*(32-(float)g_iCurrentFrac)/32 + g_Track[g_iCurrentPoint+1].eye[0]*((float)g_iCurrentFrac)/32,
		g_Track[g_iCurrentPoint].eye[1]*(32-(float)g_iCurrentFrac)/32 + g_Track[g_iCurrentPoint+1].eye[1]*((float)g_iCurrentFrac)/32,
		g_Track[g_iCurrentPoint].eye[2]*(32-(float)g_iCurrentFrac)/32 + g_Track[g_iCurrentPoint+1].eye[2]*((float)g_iCurrentFrac)/32,
		g_Track[g_iCurrentPoint].lookat[0]*(32-(float)g_iCurrentFrac)/32 + g_Track[g_iCurrentPoint+1].lookat[0]*((float)g_iCurrentFrac)/32,
		g_Track[g_iCurrentPoint].lookat[1]*(32-(float)g_iCurrentFrac)/32 + g_Track[g_iCurrentPoint+1].lookat[1]*((float)g_iCurrentFrac)/32,
		g_Track[g_iCurrentPoint].lookat[2]*(32-(float)g_iCurrentFrac)/32 + g_Track[g_iCurrentPoint+1].lookat[2]*((float)g_iCurrentFrac)/32,
		g_Track[g_iCurrentPoint].normal[0]*(32-(float)g_iCurrentFrac)/32 + g_Track[g_iCurrentPoint+1].normal[0]*((float)g_iCurrentFrac)/32,
		g_Track[g_iCurrentPoint].normal[1]*(32-(float)g_iCurrentFrac)/32 + g_Track[g_iCurrentPoint+1].normal[1]*((float)g_iCurrentFrac)/32,
		g_Track[g_iCurrentPoint].normal[2]*(32-(float)g_iCurrentFrac)/32 + g_Track[g_iCurrentPoint+1].normal[2]*((float)g_iCurrentFrac)/32);


	drawTrack();
	drawGround();
	drawSky();

	glutSwapBuffers();
}

// keyboard callback
void keyboard(unsigned char c, int x, int y)
{
	if (c=='a')
	{
		g_iSpeed = 20;
		g_iCurrentPoint = 0;
		g_iCurrentFrac = 0;
	}
	if (c=='s')
		g_bMove = !g_bMove;
	if (c=='d')
		g_bTime = !g_bTime;
}

/* Various initializations of openGL elements */
void myInit()
{
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_DEPTH | GLUT_RGBA);

	glutInitWindowSize(640, 480);
	glutInitWindowPosition(100, 100);
	glutCreateWindow("Roller Coaster");

	glShadeModel(GL_SMOOTH);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60, 1.33, 0.1, 200);
	glMatrixMode(GL_MODELVIEW);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_NORMALIZE);

	g_texGrass.LoadBmpFile("grass.bmp");
	g_texSkybox.LoadBmpFile("skybox.bmp");

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glLineWidth(3);

	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);

	GLfloat ambientLight[] = {0.2, 0.2, 0.2, 1.0};
	GLfloat diffuseLight[] = {0.8, 0.8, 0.8, 1.0};
	GLfloat specularLight[] = {1.0, 1.0, 1.0, 1.0};
	GLfloat lightPosition[] = {0.0, 1.0, 0.0, 0.0};

	glLightfv(GL_LIGHT0, GL_AMBIENT, ambientLight);	
	glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuseLight);
	glLightfv(GL_LIGHT0, GL_SPECULAR, specularLight);
	glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);

	glutIdleFunc(doIdle);
	glutDisplayFunc(display);

	glutKeyboardFunc(keyboard);

	// random terrain generation

	for (int i=0; i<65; i++)
	{
		for (int j=0; j<65; j++)
		{
			g_Terrain[i][j] = -20;
		}
	}

	// random seed
	srand(3);
	for (int t=0; t<200; t++)
	{
		double v = rand();
		double a = sin(v);
		double b = cos(v);
		double c = ((float)rand()/RAND_MAX)*90.51 - 45.25;
		for (int i=0; i<65; i++)
		{
			for (int j=0; j<65; j++)
			{
				if (a*i + b*j - c > 0)
				{
					g_Terrain[i][j] += 1;
				}
				else
				{
					g_Terrain[i][j] -= 1;
				}
			}
		}
	}

	for (int i=0; i<64; i++)
	{
		for (int j=0; j<64; j++)
		{
			float a1 = -4;
			float a2 = g_Terrain[i][j] - g_Terrain[i+1][j];
			float a3 = 0;
			float b1 = -4;
			float b2 = g_Terrain[i][j+1] - g_Terrain[i+1][j];
			float b3 = -4;
			g_TerrainNormal[i][j][0] = a2*b3-a3*b2;
			g_TerrainNormal[i][j][1] = a3*b1-a1*b3;
			g_TerrainNormal[i][j][2] = a1*b2-a2*b1;

			a1 = 0;
			a2 = g_Terrain[i+1][j] - g_Terrain[i+1][j+1];
			a3 = -4;
			b1 = -4;
			b2 = g_Terrain[i][j+1] - g_Terrain[i+1][j+1];
			b3 = 0;
			g_TerrainNormal[i][j][3] = a2*b3-a3*b2;
			g_TerrainNormal[i][j][4] = a3*b1-a1*b3;
			g_TerrainNormal[i][j][5] = a1*b2-a2*b1;
		}
	}

}

int _tmain(int argc, _TCHAR* argv[])
{
	// I've set the argv[1] to track.txt.
	// To change it, on the "Solution Explorer",
	// right click "assign1", choose "Properties",
	// go to "Configuration Properties", click "Debugging",
	// then type your track file name for the "Command Arguments"
	if (argc<2)
	{  
		printf ("usage: %s <trackfile>\n", argv[0]);
		exit(0);
	}

	loadSplines(argv[1]);
	calcSplines();

	glutInit(&argc,(char**)argv);

	myInit();

	glutMainLoop();

	return 0;
}

