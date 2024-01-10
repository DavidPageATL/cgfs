/*
   Copyright 2024 David Page

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

	   http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#define SDL_MAIN_HANDLED

#include <stdlib.h>
#include <SDL.h>

static int WIDTH = 600;
static int HEIGHT = 600;

static float VIEWPORT_SIZE = 1;
static float PROJECTION_PLANE_Z = 1;

typedef struct {
	int r;
	int g;
	int b;
} Color;

typedef struct {
	float x;
	float y;
	float z;
} Vec3d;

typedef struct {
	Vec3d center;
	float radius;
	Color color;
	float specular;
} Sphere;

enum LightType {
	Ambient, Point, Directional
};

typedef struct {
	LightType type;
	float intensity;
	Vec3d position;
} Light;

static float DotProduct(Vec3d v1, Vec3d v2) {
	return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

static Vec3d Add(Vec3d v1, Vec3d v2) {
	return Vec3d{ v1.x + v2.x, v1.y + v2.y, v1.z + v2.z };
}

static Vec3d Subtract(Vec3d v1, Vec3d v2) {
	return Vec3d{ v1.x - v2.x, v1.y - v2.y, v1.z - v2.z };
}

static Vec3d Multiply(float k, Vec3d vec) {
	return Vec3d{ k * vec.x, k * vec.y, k * vec.z };
}

static Color Multiply(float k, Color col) {
	return Color{ (int)(k * col.r), (int)(k * col.g), (int)(k * col.b) };
}

static int min(int a, int b) {
	return a < b ? a : b;
}

static int max(int a, int b) {
	return a > b ? a : b;
}

static Color Clamp(Color c) {
	return Color{
		min(255, max(0, c.r)),
		min(255, max(0, c.g)),
		min(255, max(0, c.b)),
	};
}

static float Length(Vec3d vec) {
	return (float)sqrt(DotProduct(vec, vec));
}

static Vec3d CanvasToViewport(int x, int y) {
	return Vec3d{ x * VIEWPORT_SIZE / WIDTH, y * VIEWPORT_SIZE / HEIGHT,  PROJECTION_PLANE_Z };
}

static Sphere spheres[] = {
	{{0, -1, 3}, 1, {255, 0, 0}, 500},
	{{2, 0, 4}, 1, {0, 0, 255}, 500},
	{{-2, 0,4}, 1, {0, 255, 0}, 10},
	{{0, -5001, 0}, 5000, {255, 255, 0}, 1000}
};

static Light lights[] = {
	{Ambient, 0.2f, {0,0,0}},
	{Point, 0.6f, {2, 1, 0}},
	{Directional, 0.2f, {1, 4, 4}}
};

static Vec3d IntersectRaySphere(Vec3d origin, Vec3d direction, Sphere sphere) {
	Vec3d oc = Subtract(origin, sphere.center);

	float k1 = DotProduct(direction, direction);
	float k2 = 2 * DotProduct(oc, direction);
	float k3 = DotProduct(oc, oc) - sphere.radius * sphere.radius;

	float discriminant = k2 * k2 - 4 * k1 * k3;

	if (discriminant < 0) {
		return Vec3d{ INFINITY, INFINITY, INFINITY };
	}

	float t1 = (float)(-k2 + sqrt(discriminant)) / (2 * k1);
	float t2 = (float)(-k2 - sqrt(discriminant)) / (2 * k1);

	return Vec3d{ t1, t2, 0 };
}

typedef struct {
	Sphere* closest_sphere;
	float closest_t;
} Intersection;

static Intersection ClosestIntersection(Vec3d origin, Vec3d direction, float min_t, float max_t) {
	float closest_t = INFINITY;
	Sphere* closest_sphere = NULL;

	for (int i = 0; i < sizeof(spheres) / sizeof(spheres[0]); i++) {

		Vec3d ts = IntersectRaySphere(origin, direction, spheres[i]);

		if (ts.x < closest_t && min_t < ts.x && ts.x < max_t) {
			closest_t = ts.x;
			closest_sphere = &spheres[i];
		}

		if (ts.y < closest_t && min_t < ts.y && ts.y < max_t) {
			closest_t = ts.y;
			closest_sphere = &spheres[i];
		}
	}

	return Intersection{ closest_sphere, closest_t };
}

static float ComputeLighting(Vec3d point, Vec3d normal, Vec3d view, float specular, float max_t) {
	float intensity = 0.0f;
	float length_n = Length(normal);
	float length_v = Length(view);

	for (int i = 0; i < sizeof(lights) / sizeof(lights[0]); i++) {
		Light light = lights[i];


		if (light.type == Ambient) {
			intensity += light.intensity;
		}
		else {
			Vec3d vec_l;

			if (light.type == Point) {
				vec_l = Subtract(light.position, point);
			}
			else {
				vec_l = light.position;
			}

			// Shadow
			Intersection intersection = ClosestIntersection(point, vec_l, 0.001, max_t);

			if (intersection.closest_sphere != NULL) {
				continue;
			}

			// Diffuse
			float n_dot_l = DotProduct(normal, vec_l);
			if (n_dot_l > 0) {
				intensity += light.intensity * n_dot_l / (length_n * Length(vec_l));
			}

			// Specular
			if (specular != -1) {
				Vec3d vec_r = Subtract(Multiply(2.0f * DotProduct(normal, vec_l), normal), vec_l);
				float r_dot_v = DotProduct(vec_r, view);
				if (r_dot_v > 0) {
					intensity += light.intensity * (float)pow(r_dot_v / (Length(vec_r) * length_v), specular);
				}
			}
		}
	}

	return intensity;
}

static Color TraceRay(Vec3d origin, Vec3d direction, float min_t, float max_t) {

	Intersection intersection = ClosestIntersection(origin, direction, min_t, max_t);

	if (intersection.closest_sphere == NULL) {
		return Color{ 255, 255, 255 };
	}

	Vec3d point = Add(origin, Multiply(intersection.closest_t, direction));
	Vec3d normal = Subtract(point, intersection.closest_sphere->center);
	normal = Multiply(1.0f / Length(normal), normal);

	Vec3d view = Multiply(-1, direction);
	float lighting = ComputeLighting(point, normal, view, intersection.closest_sphere->specular, max_t);
	return Multiply(lighting, intersection.closest_sphere->color);
}

static void PutPixel(SDL_Renderer* renderer, int x, int y, Color c) {
	x = WIDTH / 2 + x;
	y = HEIGHT / 2 - y - 1;

	if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) {
		return;
	}

	SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 255);
	SDL_RenderDrawPoint(renderer, x, y);
}

extern C_LINKAGE int SDL_main(int argc, char* argv[]) {

	SDL_Window* window = NULL;
	SDL_Renderer* renderer = NULL;

	if (SDL_Init(SDL_INIT_VIDEO) != 0) {
		SDL_Log("SDL failed to initialise: %s\n", SDL_GetError());
		return 1;
	}

	if (SDL_CreateWindowAndRenderer(WIDTH, HEIGHT, 0, &window, &renderer) != 0) {
		SDL_Log("Failed to create window and renderer %s\n", SDL_GetError());
		return EXIT_FAILURE;
	}

	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 0);
	SDL_RenderClear(renderer);

	Vec3d camera_position{ 0, 0, 0 };

	for (int x = -WIDTH / 2; x < WIDTH / 2; x++) {
		for (int y = -HEIGHT / 2; y < HEIGHT / 2; y++) {
			Vec3d direction = CanvasToViewport(x, y);
			Color color = Clamp(TraceRay(camera_position, direction, 1, INFINITY));
			PutPixel(renderer, x, y, color);
		}
	}

	SDL_RenderPresent(renderer);

	while (true) {
		SDL_Event event;
		if (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT) {
				break;
			}
		}
	}

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);

	SDL_Quit();

	return EXIT_SUCCESS;
}