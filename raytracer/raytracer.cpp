#include <pic.h>
#include <windows.h>
#include <stdlib.h>
#include <GL/glu.h>
#include <GL/glut.h>

#include <stdio.h>
#include <string>
#include <cmath>
#define MAX_TRIANGLES 190
#define MAX_SPHERES 10
#define MAX_LIGHTS 10

char *filename=0;

//different display modes
#define MODE_DISPLAY 1
#define MODE_JPEG 2
int mode=MODE_DISPLAY;

#define WIDTH 640
#define HEIGHT 480


//the field of view of the camera
#define fov 60.0

unsigned char buffer[HEIGHT][WIDTH][3];

struct Vertex
{
	double position[3];
	double color_diffuse[3];
	double color_specular[3];
	double normal[3];
	double shininess;
};

typedef struct _Triangle
{
	struct Vertex v[3];
} Triangle;

typedef struct _Sphere
{
	double position[3];
	double color_diffuse[3];
	double color_specular[3];
	double shininess;
	double radius;
} Sphere;

typedef struct _Light
{
	double position[3];
	double color[3];
} Light;

Triangle triangles[MAX_TRIANGLES];
Sphere spheres[MAX_SPHERES];
Light lights[MAX_LIGHTS];
double ambient_light[3];

int num_triangles=0;
int num_spheres=0;
int num_lights=0;

void plot_pixel_display(int x,int y,unsigned char r,unsigned char g,unsigned char b);
void plot_pixel_jpeg(int x,int y,unsigned char r,unsigned char g,unsigned char b);
void plot_pixel(int x,int y,unsigned char r,unsigned char g,unsigned char b);

bool collision_trace(double ray_dir[3], double ray_ori[3], double min_dist);
void save_jpg();


//puts AxB into C
void cross_product (double a[3], double b[3], double c[3]) {
	c[0] = a[1]*b[2]-a[2]*b[1];
	c[1] = a[2]*b[0]-a[0]*b[2];
	c[2] = a[0]*b[1]-a[1]*b[0];
}

//returns the weights a point should be colored
double * triangle_color_weights (double p[3], int t) {
	double dist[3];

	double cross[3];

	double to_cross_one[3];
	double to_cross_two[3];

	for (int i=0; i<3; i++) {
		to_cross_one[i]=triangles[t].v[1].position[i]-p[i];
		to_cross_two[i]=triangles[t].v[2].position[i]-p[i];
	}

	cross_product(to_cross_one, to_cross_two, cross);
	dist[0] = sqrt(pow(cross[0],2)+pow(cross[1],2)+pow(cross[2],2));

	for (int i=0; i<3; i++) {
		to_cross_one[i]=p[i]-triangles[t].v[0].position[i];
		to_cross_two[i]=triangles[t].v[2].position[i]-triangles[t].v[0].position[i];
	}

	cross_product(to_cross_one, to_cross_two, cross);
	dist[1] = sqrt(pow(cross[0],2)+pow(cross[1],2)+pow(cross[2],2));

	for (int i=0; i<3; i++) {
		to_cross_one[i]=triangles[t].v[1].position[i]-triangles[t].v[0].position[i];
		to_cross_two[i]=triangles[t].v[2].position[i]-triangles[t].v[0].position[i];	
	}

	cross_product(to_cross_one, to_cross_two, cross);
	
	dist[0]/= sqrt(pow(cross[0],2)+pow(cross[1],2)+pow(cross[2],2));
	dist[1]/= sqrt(pow(cross[0],2)+pow(cross[1],2)+pow(cross[2],2));
	dist[2]=1.0-dist[0]-dist[1];

	/*
	dist[0] = sqrt(pow(triangles[t].v[0].position[0]-p[0],2)+
		pow(triangles[t].v[0].position[1]-p[1],2)+
		pow(triangles[t].v[0].position[2]-p[2],2));
	dist[1] = sqrt(pow(triangles[t].v[1].position[0]-p[0],2)+
		pow(triangles[t].v[1].position[1]-p[1],2)+
		pow(triangles[t].v[1].position[2]-p[2],2));
	dist[2] = sqrt(pow(triangles[t].v[2].position[0]-p[0],2)+
		pow(triangles[t].v[2].position[1]-p[1],2)+
		pow(triangles[t].v[2].position[2]-p[2],2));
	dist[0]=1/dist[0];
	dist[1]=1/dist[1];
	dist[2]=1/dist[2];
	double weight = (dist[0]+dist[1]+dist[2]);
	dist[0]/=weight;
	dist[1]/=weight;
	dist[2]/=weight;
	*/
	return dist;
}

//calculate additional light color
void light_trace (double disp_color[3], double ray_dir[3], double ray_ori[3], double norm[3], double view_back[3], double spec[3], double diff[3], double sh) {

	for (int l=0; l<num_lights; l++) {

		double shadow_intensity=1;

		double light_dist=sqrt(pow(lights[l].position[0]-ray_ori[0],2)+
			pow(lights[l].position[1]-ray_ori[1],2)+
			pow(lights[l].position[2]-ray_ori[2],2));

		double light_dir[3];
		double diff_color[3];

		for (int x_shift=-1; x_shift<2; x_shift++) {
			for (int y_shift=-1; y_shift<2; y_shift++) {
				for (int z_shift=-1; z_shift<2; z_shift++) {
					light_dir[0]=lights[l].position[0]-ray_ori[0]+(double)x_shift/20.0;
					light_dir[1]=lights[l].position[1]-ray_ori[1]+(double)y_shift/20.0;
					light_dir[2]=lights[l].position[2]-ray_ori[2]+(double)z_shift/20.0;
					double weight=sqrt(pow(light_dir[0],2)+pow(light_dir[1],2)+pow(light_dir[2],2));
					light_dir[0]/=weight;
					light_dir[1]/=weight;
					light_dir[2]/=weight;

					if (collision_trace(light_dir, ray_ori, light_dist)) {
						shadow_intensity-=1.0/27.0;
					}
				}
			}
		}



		double diff_coeff=light_dir[0]*norm[0]+light_dir[1]*norm[1]+light_dir[2]*norm[2];

		double light_ref[3];
		for (int i=0; i<3; i++) {
			light_ref[i]=diff_coeff*2*norm[i]-light_dir[i];
		}

		double spec_color[3];
		double spec_coeff=view_back[0]*light_ref[0]+view_back[1]*light_ref[1]+view_back[2]*light_ref[2];

		if (spec_coeff > 0)
			spec_coeff=pow(spec_coeff,sh);
		else
			spec_coeff=0;

		for (int i=0; i<3; i++) {
			spec_color[i]=spec[i]*spec_coeff;
			diff_color[i]=diff[i]*diff_coeff;
			if (spec_color[i] < 0) {
				spec_color[i] = 0;
			}
			if (diff_color[i] < 0) {
				diff_color[i] = 0;
			}
			disp_color[i]+=(spec_color[i]+diff_color[i])*shadow_intensity;
			if (disp_color[i] > 255) {
				disp_color[i]=255;
			}
		}
	}
}

//check for collisions
bool collision_trace (double ray_dir[3], double ray_ori[3], double min_dist) {

	ray_ori[0]+=ray_dir[0]*0.01;
	ray_ori[1]+=ray_dir[1]*0.01;
	ray_ori[2]+=ray_dir[2]*0.01;

	for (int i=0; i<num_spheres; i++) {

		double coeff[3];
		double intersect_dist[2];

		coeff[0]=1;
		coeff[1]=2*(ray_dir[0]*(ray_ori[0]-spheres[i].position[0]) 
			+ ray_dir[1]*(ray_ori[1]-spheres[i].position[1]) 
			+ ray_dir[2]*(ray_ori[2]-spheres[i].position[2]));
		coeff[2]=pow(ray_ori[0]-spheres[i].position[0],2)
			+ pow(ray_ori[1]-spheres[i].position[1],2)
			+ pow(ray_ori[2]-spheres[i].position[2],2)
			- pow(spheres[0].radius, 2);

		intersect_dist[0] = pow(coeff[1],2)-4*coeff[2];
		if (intersect_dist[0] > 0)
			intersect_dist[0] = (-1*coeff[1]+sqrt(intersect_dist[0]))/2;
		else
			intersect_dist[0] = 0;

		intersect_dist[1] = pow(coeff[1],2)-4*coeff[2];
		if (intersect_dist[1] > 0)
			intersect_dist[1] = (-1*coeff[1]-sqrt(intersect_dist[1]))/2;
		else
			intersect_dist[1] = 0;

		if (intersect_dist[0] > 0 && intersect_dist[1] > 0 && min(intersect_dist[0], intersect_dist[1])<min_dist) {
			return true;
		}
	}

	//check against triangles
	for (int i=0; i<num_triangles; i++) {

		double plane_coeff[4];
		double intersect[3];
		double point_in_value;
		double dist;

		bool point_in_test=true;

		//calculate plane coefficients
		plane_coeff[0] = 
			(triangles[i].v[1].position[1]-triangles[i].v[0].position[1])
			*(triangles[i].v[2].position[2]-triangles[i].v[0].position[2])
			-(triangles[i].v[2].position[1]-triangles[i].v[0].position[1])
			*(triangles[i].v[1].position[2]-triangles[i].v[0].position[2]);
		plane_coeff[1] = 
			(triangles[i].v[1].position[2]-triangles[i].v[0].position[2])
			*(triangles[i].v[2].position[0]-triangles[i].v[0].position[0])
			-(triangles[i].v[2].position[2]-triangles[i].v[0].position[2])
			*(triangles[i].v[1].position[0]-triangles[i].v[0].position[0]);
		plane_coeff[2] = 
			(triangles[i].v[1].position[0]-triangles[i].v[0].position[0])
			*(triangles[i].v[2].position[1]-triangles[i].v[0].position[1])
			-(triangles[i].v[2].position[0]-triangles[i].v[0].position[0])
			*(triangles[i].v[1].position[1]-triangles[i].v[0].position[1]);
		plane_coeff[3] = -1*(
			(plane_coeff[0]*triangles[i].v[0].position[0])
			+(plane_coeff[1]*triangles[i].v[0].position[1])
			+(plane_coeff[2]*triangles[i].v[0].position[2]));

		//normalize the coefficients
		double weight = sqrt(pow(plane_coeff[0],2)+pow(plane_coeff[1],2)+pow(plane_coeff[2],2));
		plane_coeff[0]/=weight;
		plane_coeff[1]/=weight;
		plane_coeff[2]/=weight;
		plane_coeff[3]/=weight;

		//calculate distance to plane
		dist = -1*(plane_coeff[0]*ray_ori[0]+plane_coeff[1]*ray_ori[1]+plane_coeff[2]*ray_ori[2]+plane_coeff[3])
			/(plane_coeff[0]*ray_dir[0]+plane_coeff[1]*ray_dir[1]+plane_coeff[2]*ray_dir[2]);

		//if the plane is ahead of the ray
		if (dist > 0) {
			intersect[0] = ray_ori[0] + ray_dir[0]*dist;
			intersect[1] = ray_ori[1] + ray_dir[1]*dist;
			intersect[2] = ray_ori[2] + ray_dir[2]*dist;

			double check_a[3];
			double check_b[3];
			double check_c[3];

			check_a[0] = triangles[i].v[1].position[0]-triangles[i].v[0].position[0];
			check_a[1] = triangles[i].v[1].position[1]-triangles[i].v[0].position[1];
			check_a[2] = triangles[i].v[1].position[2]-triangles[i].v[0].position[2];
			check_b[0] = intersect[0]-triangles[i].v[0].position[0];
			check_b[1] = intersect[1]-triangles[i].v[0].position[1];
			check_b[2] = intersect[2]-triangles[i].v[0].position[2];

			cross_product(check_a, check_b, check_c);

			point_in_value = check_c[0]*plane_coeff[0]+check_c[1]*plane_coeff[1]+check_c[2]*plane_coeff[2];
			if (point_in_value<=0)
				point_in_test=false;

			check_a[0] = triangles[i].v[2].position[0]-triangles[i].v[1].position[0];
			check_a[1] = triangles[i].v[2].position[1]-triangles[i].v[1].position[1];
			check_a[2] = triangles[i].v[2].position[2]-triangles[i].v[1].position[2];
			check_b[0] = intersect[0]-triangles[i].v[1].position[0];
			check_b[1] = intersect[1]-triangles[i].v[1].position[1];
			check_b[2] = intersect[2]-triangles[i].v[1].position[2];
			
			cross_product(check_a, check_b, check_c);

			point_in_value = check_c[0]*plane_coeff[0]+check_c[1]*plane_coeff[1]+check_c[2]*plane_coeff[2];
			if (point_in_value<=0)
				point_in_test=false;

			check_a[0] = triangles[i].v[0].position[0]-triangles[i].v[2].position[0];
			check_a[1] = triangles[i].v[0].position[1]-triangles[i].v[2].position[1];
			check_a[2] = triangles[i].v[0].position[2]-triangles[i].v[2].position[2];
			check_b[0] = intersect[0]-triangles[i].v[2].position[0];
			check_b[1] = intersect[1]-triangles[i].v[2].position[1];
			check_b[2] = intersect[2]-triangles[i].v[2].position[2];
			
			cross_product(check_a, check_b, check_c);

			point_in_value = check_c[0]*plane_coeff[0]+check_c[1]*plane_coeff[1]+check_c[2]*plane_coeff[2];
			if (point_in_value<=0)
				point_in_test=false;

			if (point_in_test && dist<min_dist)
				return true;
		}
	}
	return false;
}

//determines the color of a ray trace
void trace (double disp_color[3], double ray_dir[3], double ray_ori[3], int times) {

	double inter_dir[3];
	double inter_ori[3];
	double spec[3];
	double diff[3];
	double norm[3];
	double sh;
	double min_dist=1000;

	//check for sphere collisions
	for (int i=0; i<num_spheres; i++) {

		double coeff[3];
		double intersect_dist[2];

		coeff[0]=1;
		coeff[1]=2*(ray_dir[0]*(ray_ori[0]-spheres[i].position[0]) 
			+ ray_dir[1]*(ray_ori[1]-spheres[i].position[1]) 
			+ ray_dir[2]*(ray_ori[2]-spheres[i].position[2]));
		coeff[2]=pow(ray_ori[0]-spheres[i].position[0],2)
			+ pow(ray_ori[1]-spheres[i].position[1],2)
			+ pow(ray_ori[2]-spheres[i].position[2],2)
			- pow(spheres[0].radius, 2);

		intersect_dist[0] = pow(coeff[1],2)-4*coeff[2];
		if (intersect_dist[0] > 0)
			intersect_dist[0] = (-1*coeff[1]+sqrt(intersect_dist[0]))/2;
		else
			intersect_dist[0] = 0;

		intersect_dist[1] = pow(coeff[1],2)-4*coeff[2];
		if (intersect_dist[1] > 0)
			intersect_dist[1] = (-1*coeff[1]-sqrt(intersect_dist[1]))/2;
		else
			intersect_dist[1] = 0;

		//if the sphere is the closest point so far
		if (intersect_dist[0] > 0 && intersect_dist[1] > 0 && min(intersect_dist[0],intersect_dist[1])<min_dist) {
			min_dist = min(intersect_dist[0],intersect_dist[1]);

			sh = spheres[i].shininess;

			for (int j=0; j<3; j++) {
				disp_color[j]=spheres[i].color_diffuse[j]*255;
				diff[j]=disp_color[j];
				spec[j]=spheres[i].color_specular[j]*255;
				inter_ori[j]=ray_ori[j]+ray_dir[j]*min_dist;
				norm[j]=inter_ori[j]-spheres[i].position[j];
			}

			double weight=sqrt(pow(norm[0],2)+pow(norm[1],2)+pow(norm[2],2));
			norm[0]/=weight;
			norm[1]/=weight;
			norm[2]/=weight;
		}
	}

	//calculate for collisions with triangles
	for (int i=0; i<num_triangles; i++) {

		double plane_coeff[4];
		double intersect[3];
		double point_in_value;
		double dist;

		bool point_in_test=true;

		//calculate plane coefficients
		plane_coeff[0] = 
			(triangles[i].v[1].position[1]-triangles[i].v[0].position[1])
			*(triangles[i].v[2].position[2]-triangles[i].v[0].position[2])
			-(triangles[i].v[2].position[1]-triangles[i].v[0].position[1])
			*(triangles[i].v[1].position[2]-triangles[i].v[0].position[2]);
		plane_coeff[1] = 
			(triangles[i].v[1].position[2]-triangles[i].v[0].position[2])
			*(triangles[i].v[2].position[0]-triangles[i].v[0].position[0])
			-(triangles[i].v[2].position[2]-triangles[i].v[0].position[2])
			*(triangles[i].v[1].position[0]-triangles[i].v[0].position[0]);
		plane_coeff[2] = 
			(triangles[i].v[1].position[0]-triangles[i].v[0].position[0])
			*(triangles[i].v[2].position[1]-triangles[i].v[0].position[1])
			-(triangles[i].v[2].position[0]-triangles[i].v[0].position[0])
			*(triangles[i].v[1].position[1]-triangles[i].v[0].position[1]);
		plane_coeff[3] = -1*(
			(plane_coeff[0]*triangles[i].v[0].position[0])
			+(plane_coeff[1]*triangles[i].v[0].position[1])
			+(plane_coeff[2]*triangles[i].v[0].position[2]));

		//normalize the coefficients
		double weight = sqrt(pow(plane_coeff[0],2)+pow(plane_coeff[1],2)+pow(plane_coeff[2],2));
		plane_coeff[0]/=weight;
		plane_coeff[1]/=weight;
		plane_coeff[2]/=weight;
		plane_coeff[3]/=weight;

		//calculate distance to plane
		dist = -1*(plane_coeff[0]*ray_ori[0]+plane_coeff[1]*ray_ori[1]+plane_coeff[2]*ray_ori[2]+plane_coeff[3])
			/(plane_coeff[0]*ray_dir[0]+plane_coeff[1]*ray_dir[1]+plane_coeff[2]*ray_dir[2]);

		//if the plane is ahead of the ray
		if (dist > 0) {
			intersect[0] = ray_ori[0] + ray_dir[0]*dist;
			intersect[1] = ray_ori[1] + ray_dir[1]*dist;
			intersect[2] = ray_ori[2] + ray_dir[2]*dist;

			double check_a[3];
			double check_b[3];
			double check_c[3];

			check_a[0] = triangles[i].v[1].position[0]-triangles[i].v[0].position[0];
			check_a[1] = triangles[i].v[1].position[1]-triangles[i].v[0].position[1];
			check_a[2] = triangles[i].v[1].position[2]-triangles[i].v[0].position[2];
			check_b[0] = intersect[0]-triangles[i].v[0].position[0];
			check_b[1] = intersect[1]-triangles[i].v[0].position[1];
			check_b[2] = intersect[2]-triangles[i].v[0].position[2];

			cross_product(check_a, check_b, check_c);

			point_in_value = check_c[0]*plane_coeff[0]+check_c[1]*plane_coeff[1]+check_c[2]*plane_coeff[2];
			if (point_in_value<=0)
				point_in_test=false;

			check_a[0] = triangles[i].v[2].position[0]-triangles[i].v[1].position[0];
			check_a[1] = triangles[i].v[2].position[1]-triangles[i].v[1].position[1];
			check_a[2] = triangles[i].v[2].position[2]-triangles[i].v[1].position[2];
			check_b[0] = intersect[0]-triangles[i].v[1].position[0];
			check_b[1] = intersect[1]-triangles[i].v[1].position[1];
			check_b[2] = intersect[2]-triangles[i].v[1].position[2];

			cross_product(check_a, check_b, check_c);

			point_in_value = check_c[0]*plane_coeff[0]+check_c[1]*plane_coeff[1]+check_c[2]*plane_coeff[2];
			if (point_in_value<=0)
				point_in_test=false;

			check_a[0] = triangles[i].v[0].position[0]-triangles[i].v[2].position[0];
			check_a[1] = triangles[i].v[0].position[1]-triangles[i].v[2].position[1];
			check_a[2] = triangles[i].v[0].position[2]-triangles[i].v[2].position[2];
			check_b[0] = intersect[0]-triangles[i].v[2].position[0];
			check_b[1] = intersect[1]-triangles[i].v[2].position[1];
			check_b[2] = intersect[2]-triangles[i].v[2].position[2];

			cross_product(check_a, check_b, check_c);

			point_in_value = check_c[0]*plane_coeff[0]+check_c[1]*plane_coeff[1]+check_c[2]*plane_coeff[2];
			if (point_in_value<=0)
				point_in_test=false;

			if (point_in_test && dist < min_dist) {
				min_dist=dist;
				double * color_weight;
				color_weight = triangle_color_weights(intersect, i);
				sh = triangles[i].v[0].shininess;

				for (int j=0; j<3; j++) {
					inter_ori[j] = intersect[j];
					
					norm[j] = triangles[i].v[0].normal[j]*color_weight[0]+
						triangles[i].v[1].normal[j]*color_weight[1]+
						triangles[i].v[2].normal[j]*color_weight[2];
					
					//norm[j] = plane_coeff[j];
					disp_color[j] = triangles[i].v[0].color_diffuse[j]*255*color_weight[0]+
						triangles[i].v[1].color_diffuse[j]*255*color_weight[1]+
						triangles[i].v[2].color_diffuse[j]*255*color_weight[2];
					diff[j] = disp_color[j];
					spec[j] = triangles[i].v[0].color_specular[j];
				}
			}
		} 
	}
	if (min_dist<1000) {

		double ray_back[3];
		double dir_coeff=norm[0]*ray_dir[0]+norm[1]*ray_dir[1]+norm[2]*ray_dir[2];

		for (int j=0; j<3; j++) {
			disp_color[j]*=ambient_light[j];
			ray_back[j]=-1*ray_dir[j];
			inter_dir[j]=ray_dir[j]-2*dir_coeff*norm[j];
		}

		double weight = sqrt(pow(inter_dir[0],2)+pow(inter_dir[1],2)+pow(inter_dir[2],2));
		inter_dir[0]/=weight;
		inter_dir[1]/=weight;
		inter_dir[2]/=weight;

		light_trace(disp_color, inter_dir, inter_ori, norm, ray_back, spec, diff, sh);

		if (times>1) {
			double recurse_color[3];
			trace(recurse_color, inter_dir, inter_ori, 1);
			for (int j=0; j<3; j++) {
				disp_color[j]*=0.7;
				disp_color[j]+=recurse_color[j]*0.3;
			}
		}

	} else {
		disp_color[0]=0;
		disp_color[1]=0;
		disp_color[2]=0;
	}
}

//MODIFY THIS FUNCTION
void draw_scene()
{
	unsigned int x,y;

	double ray_dir[3];
	double ray_ori[3];
	double disp_color[3];
	
	double disp_color_aa1[3];
	double disp_color_aa2[3];
	double disp_color_aa3[3];


	//simple output
	for(x=0;x < WIDTH;x++)
	{
		glBegin(GL_POINTS);

		for(y=0;y < HEIGHT;y++)
		{
			for (int aa=0; aa<4; aa++) {

				//calculate the direction of the ray
				ray_dir[0]=x/320.0-1.0;
				ray_dir[1]=y/320.0-.75;
				ray_dir[2]=-1;

				switch (aa) {
				case 1:
					ray_dir[0]+=1.0/640.0;
					break;
				case 2:
					ray_dir[1]+=1.0/640.0;
					break;
				case 3:
					ray_dir[0]+=1.0/640.0;
					ray_dir[1]+=1.0/640.0;
					break;
				}

				//normalize the direction of the ray
				double total = ray_dir[0]*ray_dir[0]+ray_dir[1]*ray_dir[1]+ray_dir[2]*ray_dir[2];
				total = sqrt(total);
				ray_dir[0]/=total;
				ray_dir[1]/=total;
				ray_dir[2]/=total;

				//calculate the origin of the ray
				ray_ori[0]=0;
				ray_ori[1]=0;
				ray_ori[2]=0;

				switch(aa) {
				case 0:
					trace(disp_color, ray_dir, ray_ori, 2);
					break;
				case 1:
					trace(disp_color_aa1, ray_dir, ray_ori, 2);
					break;
				case 2:
					trace(disp_color_aa2, ray_dir, ray_ori ,2);
					break;
				case 3:
					trace(disp_color_aa3, ray_dir, ray_ori, 2);
					break;
				}
			}

			//trace(disp_color, ray_dir, ray_ori);
	
			for (int k=0; k<4; k++) {
				disp_color[k]+=disp_color_aa1[k]+disp_color_aa2[k]+disp_color_aa3[k];
				disp_color[k]/=4;
			}
			
			plot_pixel(x,y,disp_color[0],disp_color[1],disp_color[2]);
		}
		glEnd();
		glFlush();
	}
	
	save_jpg();
	printf("Done!\n"); fflush(stdout);
}

void plot_pixel_display(int x,int y,unsigned char r,unsigned char g,unsigned char b)
{
	glColor3f(((double)r)/256.f,((double)g)/256.f,((double)b)/256.f);
	glVertex2i(x,y);
}

void plot_pixel_jpeg(int x,int y,unsigned char r,unsigned char g,unsigned char b)
{
	buffer[HEIGHT-y-1][x][0]=r;
	buffer[HEIGHT-y-1][x][1]=g;
	buffer[HEIGHT-y-1][x][2]=b;
}

void plot_pixel(int x,int y,unsigned char r,unsigned char g, unsigned char b)
{
	plot_pixel_display(x,y,r,g,b);
	if(mode == MODE_JPEG)
		plot_pixel_jpeg(x,y,r,g,b);
}

void save_jpg()
{
	Pic *in = NULL;

	in = pic_alloc(640, 480, 3, NULL);
	printf("Saving JPEG file: %s\n", filename);

	memcpy(in->pix,buffer,3*WIDTH*HEIGHT);
	if (jpeg_write(filename, in))
		printf("File saved Successfully\n");
	else
		printf("Error in Saving\n");

	pic_free(in);      

}

void parse_check(char *expected,char *found)
{
	if(stricmp(expected,found))
	{
		char error[100];
		printf("Expected '%s ' found '%s '\n",expected,found);
		printf("Parse error, abnormal abortion\n");
		exit(0);
	}

}

void parse_doubles(FILE*file, char *check, double p[3])
{
	char str[100];
	fscanf(file,"%s",str);
	parse_check(check,str);
	fscanf(file,"%lf %lf %lf",&p[0],&p[1],&p[2]);
	printf("%s %lf %lf %lf\n",check,p[0],p[1],p[2]);
}

void parse_rad(FILE*file,double *r)
{
	char str[100];
	fscanf(file,"%s",str);
	parse_check("rad:",str);
	fscanf(file,"%lf",r);
	printf("rad: %f\n",*r);
}

void parse_shi(FILE*file,double *shi)
{
	char s[100];
	fscanf(file,"%s",s);
	parse_check("shi:",s);
	fscanf(file,"%lf",shi);
	printf("shi: %f\n",*shi);
}

int loadScene(char *argv)
{
	FILE *file = fopen(argv,"r");
	int number_of_objects;
	char type[50];
	int i;
	Triangle t;
	Sphere s;
	Light l;
	fscanf(file,"%i",&number_of_objects);

	printf("number of objects: %i\n",number_of_objects);
	char str[200];

	parse_doubles(file,"amb:",ambient_light);

	for(i=0;i < number_of_objects;i++)
	{
		fscanf(file,"%s\n",type);
		printf("%s\n",type);
		if(stricmp(type,"triangle")==0)
		{

			printf("found triangle\n");
			int j;

			for(j=0;j < 3;j++)
			{
				parse_doubles(file,"pos:",t.v[j].position);
				parse_doubles(file,"nor:",t.v[j].normal);
				parse_doubles(file,"dif:",t.v[j].color_diffuse);
				parse_doubles(file,"spe:",t.v[j].color_specular);
				parse_shi(file,&t.v[j].shininess);
			}

			if(num_triangles == MAX_TRIANGLES)
			{
				printf("too many triangles, you should increase MAX_TRIANGLES!\n");
				exit(0);
			}
			triangles[num_triangles++] = t;
		}
		else if(stricmp(type,"sphere")==0)
		{
			printf("found sphere\n");

			parse_doubles(file,"pos:",s.position);
			parse_rad(file,&s.radius);
			parse_doubles(file,"dif:",s.color_diffuse);
			parse_doubles(file,"spe:",s.color_specular);
			parse_shi(file,&s.shininess);

			if(num_spheres == MAX_SPHERES)
			{
				printf("too many spheres, you should increase MAX_SPHERES!\n");
				exit(0);
			}
			spheres[num_spheres++] = s;
		}
		else if(stricmp(type,"light")==0)
		{
			printf("found light\n");
			parse_doubles(file,"pos:",l.position);
			parse_doubles(file,"col:",l.color);

			if(num_lights == MAX_LIGHTS)
			{
				printf("too many lights, you should increase MAX_LIGHTS!\n");
				exit(0);
			}
			lights[num_lights++] = l;
		}
		else
		{
			printf("unknown type in scene description:\n%s\n",type);
			exit(0);
		}
	}
	return 0;
}

void display()
{

}

void init()
{
	glMatrixMode(GL_PROJECTION);
	glOrtho(0,WIDTH,0,HEIGHT,1,-1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glClearColor(0,0,0,0);
	glClear(GL_COLOR_BUFFER_BIT);
}

void idle()
{
	//hack to make it only draw once
	static int once=0;
	if(!once)
	{

		draw_scene();
		if(mode == MODE_JPEG)
			save_jpg();
	}
	once=1;
}

int main (int argc, char ** argv)
{
	if (argc<2 || argc > 3)
	{  
		printf ("usage: %s <scenefile> [jpegname]\n", argv[0]);
		exit(0);
	}
	if(argc == 3)
	{
		mode = MODE_JPEG;
		filename = argv[2];
	}
	else if(argc == 2)
		mode = MODE_DISPLAY;

	glutInit(&argc,argv);
	loadScene(argv[1]);

	glutInitDisplayMode(GLUT_RGBA | GLUT_SINGLE);
	glutInitWindowPosition(0,0);
	glutInitWindowSize(WIDTH,HEIGHT);
	int window = glutCreateWindow("Ray Tracer");
	glutDisplayFunc(display);
	glutIdleFunc(idle);
	init();
	glutMainLoop();
}
