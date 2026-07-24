#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <map>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <array>
#include <sstream>
#include <fstream>
#include <random>

#include <stdio.h>
#include <math.h> // math.h is better than cmath, prove me wrong.
#include <string.h> // string.h is also better than cstring

const int tilewidth = 256;
const int tileheight = 128;

int width;
int height;

float px = 3.0f;
float py = 3.0f;

float speed = 0.05f;
int tmpx, tmpy;

const float tfps = 120.0f;
const float tfd = 1.0f / tfps;
const char* lf = "levels/test/floor.txt";
const char* lw = "levels/test/wall.txt";
const char* ls = "levels/test/special.txt";

bool paused = false;
bool ispaused = false;

void getscrxy(float x, float y, float* xout, float* yout) {
	*xout = tilewidth/2 * (x-y);
	*yout = tileheight/2 * (x+y);
}

std::vector<std::string> split(std::string input, char delimiter) {
	std::stringstream ss(input);
	std::string token;
	std::vector<std::string> output;
	while (std::getline(ss, token, delimiter)) {
		output.push_back(token);
	}
	return output;
}

std::vector<std::vector<int>> readmap(const char* filename) {
	std::ifstream readfile(filename);
	std::string line;
	std::vector<std::vector<int>> output;
	while (std::getline(readfile, line)) {
		std::vector<int> tmp;
		for (const auto& val : split(line, ' ')) {
			tmp.push_back(std::stoi(val));
		}
		output.push_back(tmp);
	}
	return output;
}

int randrange(int min, int max) {
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<int> distrib(0, max-1);
	int res = distrib(gen);
	return res;
}

typedef struct draw {
	int x;
	int y;
	int w;
	int h;
	unsigned int text;
	int tile;
	float ssh;
} draw;

unsigned int loadTexture(const char* texture) {
	int imgwidth, imgheight, channels;
	unsigned char* text = stbi_load(texture, &imgwidth, &imgheight, &channels, 4);

	// Bjarne's mom

	unsigned int textID;
	glGenTextures(1, &textID);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glBindTexture(GL_TEXTURE_2D, textID);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, imgwidth, imgheight, 0, GL_RGBA, GL_UNSIGNED_BYTE, text);
	stbi_image_free(text);
	return textID;
}

void drawImg(int x, int y, int w, int h, unsigned int textureID, int tileID, float ssh) {
	if (tileID == -1) {
		tileID = 12;
	}

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, textureID);

	int tx = tileID % 8;
	int ty = tileID / 8;

	float u1 = (32*tx) / 256.0f;
	float v1 = ((32*ty) / ssh);

	float u2 = ((32*tx) + 32.0f) / 256.0f; // you see, every single sprite sheet is 256 pixels wide and I would most likely cease to exist if any needed to be a different width.
	float v2 = ((32*ty + 32.0f) / ssh);

	// i hate my life
	// if you read this you're bi

	glBegin(GL_QUADS);
	glTexCoord2f(u1, v1);
	glVertex2f(x, y);
	glTexCoord2f(u2, v1);
	glVertex2f(x + w, y);
	glTexCoord2f(u2, v2);
	glVertex2f(x + w, y + h);
	glTexCoord2f(u1, v2);
	glVertex2f(x, y + h);
	glEnd();

	glDisable(GL_TEXTURE_2D);
}

void overlay(float w, float h, bool paused) {
	if (paused) {
		glColor4f(0.0f, 0.0f, 0.0f, 0.5f);
		glBegin(GL_QUADS);
		glVertex2f(0.0f, 0.0f);
		glVertex2f(w, 0.0f);
		glVertex2f(w, h);
		glVertex2f(0.0f, h);
		glEnd();
		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
	}
}

void drawGUI(int x1, int y1, int w, int h, unsigned int textureID, float ssh) {
	int x2 = x1+32;
	int y2 = y1+32;
	int x3 = x1+w-32;
	int y3 = y1+h-32;
	int w2 = w-64;
	int h2 = h-64;

	drawImg(x1, y1, 32, 32, textureID, 0, ssh);
	drawImg(x2, y1, w2, 32, textureID, 1, ssh);
	drawImg(x3, y1, 32, 32, textureID, 2, ssh);
	drawImg(x1, y2, 32, h2, textureID, 3, ssh);
	drawImg(x2, y2, w2, h2, textureID, 4, ssh);
	drawImg(x3, y2, 32, h2, textureID, 5, ssh);
	drawImg(x1, y3, 32, 32, textureID, 6, ssh);
	drawImg(x2, y3, w2, 32, textureID, 7, ssh);
	drawImg(x3, y3, 32, 32, textureID, 8, ssh);
}

// we live in a cruel world

int main(int argc, char* argv[]) {
	glfwInit();
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	GLFWwindow* screen = glfwCreateWindow(800, 600, "Almost There", NULL, NULL);
	glfwMakeContextCurrent(screen);
	gladLoadGL();

	glClearColor(0.2, 0.2, 0.2, 1.0);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_CULL_FACE);

	int mvmnt[] = {GLFW_KEY_E, GLFW_KEY_D, GLFW_KEY_S, GLFW_KEY_F, GLFW_KEY_ESCAPE};
	unsigned int state = 0;

	int level = 0;
	const char* fms[] = {"levels/0/floor.txt", "levels/1/floor.txt", "levels/2/floor.txt", "levels/3/floor.txt", "levels/4/floor.txt"};
	const char* wms[] = {"levels/0/wall.txt", "levels/1/wall.txt", "levels/2/wall.txt", "levels/3/wall.txt", "levels/4/wall.txt"};
	const char* sms[] = {"levels/0/special.txt", "levels/1/special.txt", "levels/2/special.txt", "levels/3/special.txt", "levels/4/special.txt"};

	std::vector<std::vector<int>> floormap = readmap(fms[level]);
	std::vector<std::vector<int>> wallmap = readmap(wms[level]);
	std::vector<std::vector<int>> specialmap = readmap(sms[level]);
	int pass = randrange(0, 1000000);


	unsigned int textID = loadTexture("assets/tiles.png");
	unsigned int playerID = loadTexture("assets/raine.png");
	unsigned int uiID = loadTexture("assets/ui.png");

	// Introducing the newest technology: The pain creator! wait no, sorry, the real name was C++.
	
	while (!glfwWindowShouldClose(screen)) {
		float fst = glfwGetTime();

		glfwGetFramebufferSize(screen, &width, &height);
		glViewport(0, 0, width, height);

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0, width, height, 0, -1, 1);

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		
		float dx = 0.0f;
		float dy = 0.0f;

		if (glfwGetKey(screen, mvmnt[0]) == GLFW_PRESS) {
			dy -= speed*3/2;
			dx -= speed*3/2;
			if (!paused) state = 1;
		}
		if (glfwGetKey(screen, mvmnt[1]) == GLFW_PRESS) {
			dy += speed*3/2;
			dx += speed*3/2;
			if (!paused) state = 0;
		}
		if (glfwGetKey(screen, mvmnt[2]) == GLFW_PRESS) {
			dy += speed;
			dx -= speed;
			if (!paused) state = 3;
		}
		if (glfwGetKey(screen, mvmnt[3]) == GLFW_PRESS) {
			dy -= speed;
			dx += speed;
			if (!paused) state = 2;
		}
		if (glfwGetKey(screen, mvmnt[4]) == GLFW_PRESS) {
			if (!ispaused) {
				paused = !(paused);
				ispaused = true;
			}
		} else {
			ispaused = false;
		}

		// WHY WHY WHY
		
		if (!paused) {
			float newx = px+dx;
			float newy = py+dy;
			if (wallmap[py+1][(int)floor(newx)] == -1 || wallmap[py+1][(int)floor(newx)] == 12) {
				px = newx;
			} 
			if (wallmap[(int)floor(newy)+1][px] == -1 || wallmap[(int)floor(newy)+1][px] == 12) { // why the fuck does adding +1 work
				py = newy;
			}
		}

		int tx = (int)floor(px);
		int ty = (int)floor(py);

		float camx = px;
		float camy = py;
		float sx, sy, csx, csy;

		std::vector<draw> buffer;
		std::vector<draw> buffer2;
		int tilesizetmp = 256;

		glClear(GL_COLOR_BUFFER_BIT);
		int ssr = 3; /*the number of rows in the tile spritesheet. change if needed.*/
		getscrxy((float)camx, (float)camy, &csx, &csy);
		for (int i=0; i<floormap.size(); i++) {
			for (int j=0; j<floormap[i].size(); j++) {
				getscrxy((float)j, (float)i, &sx, &sy);
				drawImg(sx-csx+(width/2), sy-csy+(height/2)+48, 256, 256, textID, floormap[i][j], (float)(32*ssr));
			}
		}

		for (int i=0; i<wallmap.size(); i++) {
			for (int j=0; j<wallmap[i].size(); j++) {
				getscrxy((float)j, (float)i, &sx, &sy);
				draw added{
					sx-csx+(width/2),
					sy-csy+(height/2)-40,
					tilesizetmp,
					tilesizetmp,
					textID,
					wallmap[i][j],
					(float)(32*ssr)
				};

				draw added2{
					sx-csx+(width/2),
					sy-csy+(height/2)-136,
					tilesizetmp,
					tilesizetmp,
					textID,
					wallmap[i][j],
					(float)(32*ssr)
				};
				if (sy <= (height/2)+96) {
					buffer.push_back(added);
					buffer.push_back(added2);
				} else {
					buffer2.push_back(added);
					buffer2.push_back(added2);
				}
			}
		}

		for (int i=0; i<specialmap.size(); i++) {
			for (int j=0; j<specialmap[i].size(); j++) {
				getscrxy((float)j, (float)i, &sx, &sy);
				draw added{
					sx-csx+(width/2),
					sy-csy+(height/2)-24,
					tilesizetmp,
					tilesizetmp,
					textID,
					specialmap[i][j],
					(float)(32*ssr)
				};
				if (sy <= (height/2)+96) {
					buffer.push_back(added);
				} else {
					buffer2.push_back(added);
				}
			}
		}
		
		for (int i=0; i<buffer.size(); i++) {
			drawImg(buffer[i].x, buffer[i].y, buffer[i].w, buffer[i].h, buffer[i].text, buffer[i].tile, buffer[i].ssh);
		}

		int pssr = 1;
		drawImg(width/2-96, height/2-96, 192, 192, playerID, state, (float)(32*pssr));

		for (int i=0; i<buffer2.size(); i++) {
			drawImg(buffer2[i].x, buffer2[i].y, buffer2[i].w, buffer2[i].h, buffer2[i].text, buffer2[i].tile, buffer2[i].ssh);
		}

		if (paused) {
			overlay((float)width, (float)height, paused);
		}

		glfwSwapBuffers(screen);
		glfwPollEvents();

		float fet = glfwGetTime();
		float fdu = fet - fst;
		if (fdu < tfd) {
			std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>((tfd - fdu) * 1000)));
		}
	}
	glfwTerminate();
}

