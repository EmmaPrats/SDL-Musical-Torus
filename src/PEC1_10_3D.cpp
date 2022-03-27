#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include <iostream>
#include <cmath>

#include "vector.h"
#include "matrix.h"

//Screen dimension constants
const int SCREEN_WIDTH = 640;
const int SCREEN_HEIGHT = 480;

//The window we'll be rendering to
SDL_Window* window = NULL;
//The surface contained by the window
SDL_Surface* screenSurface = NULL;

#define FPS 60
int lastTime = 0, currentTime, deltaTime;
float msFrame = 1 / (FPS / 1000.0f);

/////////////////// 3D OBJECT ///////////////////

// Texture
SDL_Surface* texture;
// buffer of 256x256 containing the light pattern (fake phong ;)
unsigned char *light;

// our 16 bit zbuffer
unsigned short *zbuffer;

// properties of our torus
#define SLICES 32
#define SPANS 16
#define EXT_RADIUS 64
#define INT_RADIUS 24

//Current rotation angles
float angleX = 0, angleY = 0, angleZ = 0;

VECTOR angularVelocity = VECTOR(0, 0, 0);

#define BASE_ANGULAR_VELOCITY 0.01f
#define ANGULAR_VELOCITY_DECAY 0.91f

float sizeModifier = 0;
float uniformScale = 0;

#define MAX_SIZE_MODIFIER 0
#define SIZE_CHANGE_SPEED 0.01f

#define MAX_UNIFORM_SCALE 1.0f
#define UNIFORM_SCALE_MODIFIER 1.01f

// we need two structures, one that holds the position of all vertices
// in object space,  and the other in screen space. the coords in world
// space doesn't need to be stored
struct
{
	VECTOR *vertices, *normals;
} org, cur;

// this structure contains all the relevant data for each poly
typedef struct
{
	int p[4];  // pointer to the vertices
	int tx[4]; // static X texture index
	int ty[4]; // static Y texture index
	VECTOR normal, centre;
} POLY;

POLY *polies;

// count values
int num_polies;
int num_vertices;

// one entry of the edge table
typedef struct {
	int x, px, py, tx, ty, z;
} edge_data;

// store two edges per horizontal line
edge_data edge_table[SCREEN_HEIGHT][2];

// remember the highest and the lowest point of the polygon
int poly_minY, poly_maxY;

// object position and orientation
MATRIX objrot;
VECTOR objpos;
MATRIX objScale;

/////////////////////////////////////////////////

///////////////////// MUSIC /////////////////////

Mix_Music *mySong;
#define BPM_MUSIC 128
#define MSEG_BPM (60000 / BPM_MUSIC)
#define FLASH_MAX_TIME 100
int flashtime = 0;
int MusicCurrentTime = 0;
int MusicCurrentTimeBeat = 0;
int MusicCurrentBeat = 0;
int MusicPreviousBeat = -1;

/////////////////////////////////////////////////

bool initSDL();
void update();
void render();

void close();
void waitTime();

void init3D();
void update3D();
void render3D();

void InitEdgeTable();
void ScanEdge(VECTOR p1, int tx1, int ty1, int px1, int py1, VECTOR p2, int tx2, int ty2, int px2, int py2);
void DrawSpan(int y, edge_data *p1, edge_data *p2);
void DrawPolies();
void init_object();
void TransformPts();

void initMusic();
void updateMusic();

int main( int argc, char* args[] )
{
	//Start up SDL and create window
	if (!initSDL())
	{
		std::cout << "Failed to initialize!\n";
		return 1;
	}
	else
	{
		IMG_Init(IMG_INIT_PNG);
		init3D();
        initMusic();

		//Main loop flag
		bool quit = false;

		//Event handler
		SDL_Event e;

		//While application is running
		while (!quit)
		{
			//Handle events on queue
			while (SDL_PollEvent(&e) != 0)
			{
				if (e.type == SDL_KEYDOWN) {
					if (e.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
						quit = true;
					}
				}
				//User requests quit
				if (e.type == SDL_QUIT)
				{
					quit = true;
				}
			}

			// updates all
			update();

			//Render
			render();

			//Update the surface
			SDL_UpdateWindowSurface(window);
			waitTime();
		}
	}

	//Free resources and close SDL
	close();

	return 0;
}

bool initSDL() {

	//Initialize SDL
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		std::cout << "SDL could not initialize! SDL_Error: %s\n" << SDL_GetError();
		return false;
	}
	//Create window
	window = SDL_CreateWindow("SDL Tutorial", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);

	if (window == NULL)
	{
		std::cout << "Window could not be created! SDL_Error: %s\n" << SDL_GetError();
		return false;
	}
	//Get window surface
	screenSurface = SDL_GetWindowSurface(window);
	return true;
}

void update()
{
    updateMusic();
	update3D();
}

void render() {

	render3D();
}

void close() {
	SDL_FreeSurface(texture);
	free(zbuffer);
	free(org.vertices);
	free(org.normals);
	free(cur.vertices);
	free(cur.normals);
	free(polies);
	//Destroy window
	SDL_DestroyWindow(window);
	//Quit SDL subsystems
	SDL_Quit();
}

void waitTime() {
	currentTime = SDL_GetTicks();
	deltaTime = currentTime - lastTime;
	if (deltaTime < (int)msFrame) {
		SDL_Delay((int)msFrame - deltaTime);
	}
	lastTime = currentTime;

}

void init3D() {
	// Load Texture
	SDL_Surface *temp = IMG_Load("resources/texture_torus.png");
	if (temp == NULL) {
		std::cout << "Image can be loaded! " << IMG_GetError();
		close();
		exit(1);
	}
	texture = SDL_ConvertSurfaceFormat(temp, SDL_PIXELFORMAT_ARGB8888, 0);

	// prepare the lighting
	light = new unsigned char[256 * 256];
	for (int j = 0; j<256; j++)
	{
		for (int i = 0; i<256; i++)
		{
			// calculate distance from the centre
			int c = ((128 - i)*(128 - i) + (128 - j)*(128 - j)) / 35;
			// check for overflow
			if (c>255) c = 255;
			// store lumel
			light[(j << 8) + i] = 255 - c;
		}
	}
	// prepare 3D data
	zbuffer = (unsigned short*) malloc(SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(unsigned short));
	init_object();

}

void initMusic()
{
    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096);
    Mix_Init(MIX_INIT_OGG);
    mySong =  Mix_LoadMUS("resources/Blastculture-Gravitation.ogg");
    if (!mySong)
    {
        std::cout << "Error loading Music: " << Mix_GetError() << std::endl;
        close();
        exit(1);
    }
    Mix_PlayMusic(mySong,0);
    flashtime = 0;
    MusicCurrentTime = 0;
    MusicCurrentTimeBeat = 0;
    MusicCurrentBeat = 0;
    MusicPreviousBeat = -1;

    sizeModifier = MAX_SIZE_MODIFIER;
    uniformScale = MAX_UNIFORM_SCALE;
}

void updateMusic()
{
    MusicCurrentTime += deltaTime;
    MusicCurrentTimeBeat += deltaTime;
    MusicPreviousBeat = MusicCurrentBeat;
    if (MusicCurrentTimeBeat >= MSEG_BPM)
    {
        MusicCurrentTimeBeat = 0;
        MusicCurrentBeat ++;
        flashtime = FLASH_MAX_TIME;
    }
    if (flashtime > 0)
    {
        flashtime -= deltaTime;
    }
    else
    {
        flashtime = 0;
    }
    if (!Mix_PlayingMusic())
    {
        close();
        exit(0);
    }
}

void update3D()
{
	memset(zbuffer, 255, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(unsigned short));

    if (MusicCurrentTime <= MSEG_BPM * 12)//9350)
    {
        uniformScale = 1;

        if (MusicPreviousBeat != MusicCurrentBeat)
        {
            sizeModifier = MAX_SIZE_MODIFIER;
        }
        else
        {
            sizeModifier += SIZE_CHANGE_SPEED * deltaTime;
        }

        angleX = M_PI_2;
        angleY = 0;
        angleZ = 0;
    }
    else if (MusicCurrentTime <= MSEG_BPM * 20)
    {
        uniformScale = 1;

        if (MusicPreviousBeat != MusicCurrentBeat)
        {
            sizeModifier = MAX_SIZE_MODIFIER;
        }
        else
        {
            sizeModifier += SIZE_CHANGE_SPEED * deltaTime;
        }

        angleX = M_PI_2;
        angleY = 0;
        angleZ += 0.25f * BASE_ANGULAR_VELOCITY * deltaTime;
    }
    else
    {
        sizeModifier = 0;

        if (MusicPreviousBeat != MusicCurrentBeat)
        {
            angularVelocity[0] = rand() % 10;
            angularVelocity[1] = rand() % 10;
            angularVelocity[2] = rand() % 10;
            angularVelocity.setMagnitude(BASE_ANGULAR_VELOCITY);

            uniformScale = MAX_UNIFORM_SCALE;
        }
        else
        {
            if (MusicCurrentBeat % 2 == 0)
                uniformScale *= UNIFORM_SCALE_MODIFIER;
            else
                uniformScale *= 0.99f;
        }

        angularVelocity = angularVelocity * ANGULAR_VELOCITY_DECAY;

        angleX += angularVelocity[0] * deltaTime;
        angleY += angularVelocity[1] * deltaTime;
        angleZ += angularVelocity[2] * deltaTime;
    }

	objpos = VECTOR(0, 0, 250);
    objrot = rotX(angleX) * rotY(angleY) * rotZ(angleZ);
    objScale = scale(uniformScale);

	TransformPts();
}

void render3D() {
	// clear the background
	SDL_FillRect(screenSurface, NULL, 0);
	// and draw the polygons
	DrawPolies();
}

/*
* clears all entries in the edge table
*/
void InitEdgeTable()
{
	for (int i = 0; i<SCREEN_HEIGHT; i++)
	{
		edge_table[i][0].x = -1;
		edge_table[i][1].x = -1;
	}
	poly_minY = SCREEN_HEIGHT;
	poly_maxY = -1;
}

/*
* scan along one edge of the poly, i.e. interpolate all values and store
* in the edge table
*/
void ScanEdge(VECTOR p1, int tx1, int ty1, int px1, int py1,
	VECTOR p2, int tx2, int ty2, int px2, int py2)
{
	// we can't handle this case, so we recall the proc with reversed params
	// saves having to swap all the vars, but it's not good practice
	if (p2[1]<p1[1]) {
		ScanEdge(p2, tx2, ty2, px2, py2, p1, tx1, ty1, px1, py1);
		return;
	}
	// convert to fixed point
	int x1 = (int)(p1[0] * 65536),
		y1 = (int)(p1[1]),
		z1 = (int)(p1[2] * 16),
		x2 = (int)(p2[0] * 65536),
		y2 = (int)(p2[1]),
		z2 = (int)(p2[2] * 16);
	// update the min and max of the current polygon
	if (y1<poly_minY) poly_minY = y1;
	if (y2>poly_maxY) poly_maxY = y2;
	// compute deltas for interpolation
	int dy = y2 - y1;
	if (dy == 0) return;
	int dx = (x2 - x1) / dy,                // assume 16.16 fixed point
		dtx = (tx2 - tx1) / dy,
		dty = (ty2 - ty1) / dy,
		dpx = (px2 - px1) / dy,
		dpy = (py2 - py1) / dy,
		dz = (z2 - z1) / dy;              // probably 12.4, but doesn't matter
										  // interpolate along the edge
	for (int y = y1; y<y2; y++)
	{
		// don't go out of the screen
		if (y>(SCREEN_HEIGHT - 1)) return;
		// only store if inside the screen, we should really clip
		if (y >= 0)
		{
			// is first slot free?
			if (edge_table[y][0].x == -1)
			{ // if so, use that
				edge_table[y][0].x = x1;
				edge_table[y][0].tx = tx1;
				edge_table[y][0].ty = ty1;
				edge_table[y][0].px = px1;
				edge_table[y][0].py = py1;
				edge_table[y][0].z = z1;
			}
			else { // otherwise use the other
				edge_table[y][1].x = x1;
				edge_table[y][1].tx = tx1;
				edge_table[y][1].ty = ty1;
				edge_table[y][1].px = px1;
				edge_table[y][1].py = py1;
				edge_table[y][1].z = z1;
			}
		}
		// interpolate our values
		x1 += dx;
		px1 += dpx;
		py1 += dpy;
		tx1 += dtx;
		ty1 += dty;
		z1 += dz;
	}
}

/*
* draw a horizontal double textured span
*/
void DrawSpan(int y, edge_data *p1, edge_data *p2)
{
	// quick check, if facing back then draw span in the other direction,
	// avoids having to swap all the vars... not a very elegant
	if (p1->x > p2->x)
	{
		DrawSpan(y, p2, p1);
		return;
	};
	// load starting points
	int z1 = p1->z,
		px1 = p1->px,
		py1 = p1->py,
		tx1 = p1->tx,
		ty1 = p1->ty,
		x1 = p1->x >> 16,
		x2 = p2->x >> 16;
	// check if it's inside the screen
	if ((x1>(SCREEN_WIDTH - 1)) || (x2<0)) return;
	// compute deltas for interpolation
	int dx = x2 - x1;
	if (dx == 0) return;
	int dtx = (p2->tx - p1->tx) / dx,  // assume 16.16 fixed point
		dty = (p2->ty - p1->ty) / dx,
		dpx = (p2->px - p1->px) / dx,
		dpy = (p2->py - p1->py) / dx,
		dz = (p2->z - p1->z) / dx;

	// setup the offsets in the buffers
	Uint8 *dst;
	Uint8 *initbuffer = (Uint8 *)screenSurface->pixels;
	int bpp = screenSurface->format->BytesPerPixel;
	Uint8 *imagebuffer = (Uint8 *)texture->pixels;
	int bppImage = texture->format->BytesPerPixel;

	// get destination offset in buffer
	long offs = y * SCREEN_WIDTH + x1;
	// loop for all pixels concerned
	for (int i = x1; i<x2; i++)
	{
		if (i>(SCREEN_WIDTH - 1)) return;
		// check z buffer
		if (i >= 0) if (z1<zbuffer[offs])
		{
			// if visible load the texel from the translated texture
			Uint8 *p = (Uint8 *)imagebuffer + ((ty1 >> 16) & 0xff) * texture->pitch + ((tx1 >> 16) & 0xFF) * bppImage;
			SDL_Color ColorTexture;
			SDL_GetRGB(*(Uint32*)(p), texture->format, &ColorTexture.r, &ColorTexture.g, &ColorTexture.b);
			// and the texel from the light map
			unsigned char LightFactor = light[((py1 >> 8) & 0xff00) + ((px1 >> 16) & 0xff)];
			// mix them together, and store
			int ColorR = (ColorTexture.r + LightFactor);
			if (ColorR > 255) 
				ColorR = 255;
			int ColorG = (ColorTexture.g + LightFactor);
			if (ColorG > 255) 
				ColorG = 255;
			int ColorB = (ColorTexture.b + LightFactor);
			if (ColorB > 255) 
				ColorB = 255;
			Uint32 resultColor = 0xFF000000 | (ColorR << 16) | (ColorG << 8) | ColorB;
			dst = initbuffer + y *screenSurface->pitch + i * bpp;
			*(Uint32 *)dst = resultColor;
			// and update the zbuffer
			zbuffer[offs] = z1;
		}
		// interpolate our values
		px1 += dpx;
		py1 += dpy;
		tx1 += dtx;
		ty1 += dty;
		z1 += dz;
		// and find next pixel
		offs++;
	}
}

/*
* cull and draw the visible polies
*/
void DrawPolies()
{
	int i;
	for (int n = 0; n<num_polies; n++)
	{
		// rotate the centre and normal of the poly to check if it is actually visible.
		VECTOR ncent = objrot * polies[n].centre,
			nnorm = objrot * polies[n].normal;

		// calculate the dot product, and check it's sign
		if ((ncent[0] + objpos[0])*nnorm[0]
			+ (ncent[1] + objpos[1])*nnorm[1]
			+ (ncent[2] + objpos[2])*nnorm[2]<0)
		{
			// the polygon is visible, so setup the edge table
			InitEdgeTable();
			// process all our edges
			for (i = 0; i<4; i++)
			{
				ScanEdge(
					// the vertex in screen space
					cur.vertices[polies[n].p[i]],
					// the static texture coordinates
					polies[n].tx[i], polies[n].ty[i],
					// the dynamic text coords computed with the normals
					(int)(65536 * (128 + 127 * cur.normals[polies[n].p[i]][0])),
					(int)(65536 * (128 + 127 * cur.normals[polies[n].p[i]][1])),
					// second vertex in screen space
					cur.vertices[polies[n].p[(i + 1) & 3]],
					// static text coords
					polies[n].tx[(i + 1) & 3], polies[n].ty[(i + 1) & 3],
					// dynamic texture coords
					(int)(65536 * (128 + 127 * cur.normals[polies[n].p[(i + 1) & 3]][0])),
					(int)(65536 * (128 + 127 * cur.normals[polies[n].p[(i + 1) & 3]][1]))
				);
			}
			// quick clipping
			if (poly_minY<0) poly_minY = 0;
			if (poly_maxY>SCREEN_HEIGHT) poly_maxY = SCREEN_HEIGHT;
			// do we have to draw anything?
			if ((poly_minY<poly_maxY) && (poly_maxY>0) && (poly_minY<SCREEN_HEIGHT))
			{
				// if so just draw relevant lines
				for (i = poly_minY; i<poly_maxY; i++)
				{
					DrawSpan(i, &edge_table[i][0], &edge_table[i][1]);
				}
			}
		}
	}
}

/*
* generate a torus object
*/
void init_object()
{
	// allocate necessary memory for points and their normals
	num_vertices = SLICES*SPANS;
	org.vertices = new VECTOR[num_vertices];
	cur.vertices = new VECTOR[num_vertices];
	org.normals = new VECTOR[num_vertices];
	cur.normals = new VECTOR[num_vertices];
	int i, j, k = 0;
	// now create all the points and their normals, start looping
	// round the origin (circle C1)
	for (i = 0; i<SLICES; i++)
	{
		// find angular position
		float ext_angle = (float)i*M_PI*2.0f / SLICES,
			ca = cos(ext_angle),
			sa = sin(ext_angle);
		// now loop round C2
		for (j = 0; j<SPANS; j++)
		{
			float int_angle = (float)j*M_PI*2.0f / SPANS,
				int_rad = EXT_RADIUS + INT_RADIUS * cos(int_angle);
			// compute position of vertex by rotating it round C1
			org.vertices[k] = VECTOR(
				int_rad * ca,
				INT_RADIUS*sin(int_angle),
				int_rad * sa);
            cur.vertices[k] = org.vertices[k];
			// then find the normal, i.e. the normalised vector representing the
			// distance to the correpsonding point on C1
			org.normals[k] = normalize(org.vertices[k] - VECTOR(EXT_RADIUS*ca, 0, EXT_RADIUS*sa));
            cur.normals[k] = org.normals[k];
			k++;
		}
	}

	// now initialize the polygons, there are as many quads as vertices
	num_polies = SPANS*SLICES;
	polies = new POLY[num_polies];
	// perform the same loop
	for (i = 0; i<SLICES; i++)
	{
		for (j = 0; j<SPANS; j++)
		{
			POLY &P = polies[i*SPANS + j];

			// setup the pointers to the 4 concerned vertices
			P.p[0] = i*SPANS + j;
			P.p[1] = i*SPANS + ((j + 1) % SPANS);
			P.p[3] = ((i + 1) % SLICES)*SPANS + j;
			P.p[2] = ((i + 1) % SLICES)*SPANS + ((j + 1) % SPANS);

			// now compute the static texture refs (X)
			P.tx[0] = (i * 512 / SLICES) << 16;
			P.tx[1] = (i * 512 / SLICES) << 16;
			P.tx[3] = ((i + 1) * 512 / SLICES) << 16;
			P.tx[2] = ((i + 1) * 512 / SLICES) << 16;

			// now compute the static texture refs (Y)
			P.ty[0] = (j * 512 / SPANS) << 16;
			P.ty[1] = ((j + 1) * 512 / SPANS) << 16;
			P.ty[3] = (j * 512 / SPANS) << 16;
			P.ty[2] = ((j + 1) * 512 / SPANS) << 16;

			// get the normalized diagonals
			VECTOR d1 = normalize(org.vertices[P.p[2]] - org.vertices[P.p[0]]),
				d2 = normalize(org.vertices[P.p[3]] - org.vertices[P.p[1]]),
				// and their dot product
				temp = VECTOR(d1[1] * d2[2] - d1[2] * d2[1],
					d1[2] * d2[0] - d1[0] * d2[2],
					d1[0] * d2[1] - d1[1] * d2[0]);
			// normalize that and we get the face's normal
			P.normal = normalize(temp);

			// the centre of the face is just the average of the 4 corners
			// we could use this for depth sorting
			temp = org.vertices[P.p[0]] + org.vertices[P.p[1]]
				+ org.vertices[P.p[2]] + org.vertices[P.p[3]];
			P.centre = VECTOR(temp[0] * 0.25, temp[1] * 0.25, temp[2] * 0.25);
		}
	}
}

/*
* rotate and project all vertices, and just rotate point normals
*/
void TransformPts()
{
    for (int i = 0; i<num_vertices; i++)
    {
        cur.vertices[i] = org.vertices[i] + org.normals[i] * sizeModifier;
        cur.vertices[i] = objScale * cur.vertices[i];

        // perform rotation
        cur.normals[i] = objrot * org.normals[i];
        cur.vertices[i] = objrot * cur.vertices[i];
        // now project onto the screen
        cur.vertices[i][2] += objpos[2];
        cur.vertices[i][0] = SCREEN_HEIGHT * (cur.vertices[i][0] + objpos[0]) / cur.vertices[i][2] + (SCREEN_WIDTH / 2);
        cur.vertices[i][1] = SCREEN_HEIGHT * (cur.vertices[i][1] + objpos[1]) / cur.vertices[i][2] + (SCREEN_HEIGHT /2);
    }
}
