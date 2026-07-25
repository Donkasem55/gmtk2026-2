#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <AL/al.h>
#include <AL/alc.h>

#include <map>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <array>
#include <sstream>
#include <iostream>
#include <fstream>
#include <random>

#include <stdio.h>
#include <math.h> // math.h is better than cmath, prove me wrong.
#include <string.h> // string.h is also better than cstring
#include <stdlib.h> // MALLOC ON TOP

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

bool paused = false;
bool ispaused = false;

bool ingui = false;
bool isingui = false;

bool holding = false;
bool isholding = false;
bool clickhold = false;

bool inmenu = false;
bool inprol = true;
unsigned volume = 100;

bool won = false;
double timewon;

bool dead = false;
char guess[7] = "000000";

void getscrxy(float x, float y, float* xout, float* yout) {
	*xout = tilewidth/2 * (x-y);
	*yout = tileheight/2 * (x+y);
}

std::vector<std::string> split(std::string input, char delimiter) {
	std::stringstream ss(input);
	std::string token;
	std::vector<std::string> output;
	while (std::getline(ss, token, delimiter)) {
		if (token != "" && token != " ") {
			output.push_back(token);
		}
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
	readfile.close();
	return output;
}

int finished = 0;
void loadprog() {
	std::ifstream readfile("savedata.txt");
	std::string tmp;
	readfile >> tmp;
	finished = std::stoi(tmp);
	readfile.close();
	return;
}

void saveprog() {
	std::ofstream writefile("savedata.txt");
	char tmp[2];
	snprintf(tmp, 2, "%d", finished);
	writefile << tmp;
	return;
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

void drawChar(int x, int y, int w, int h, unsigned int textureID, int tileID, float ssh, float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f) {
	if (tileID == -1) {
		tileID = 12;
	}

	glEnable(GL_TEXTURE_2D);

	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_RGB, GL_PRIMARY_COLOR);
	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_RGB, GL_PRIMARY_COLOR);
	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
	glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
	glTexEnvi(GL_TEXTURE_ENV, GL_SRC0_ALPHA, GL_TEXTURE);
	glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
	glColor4f(r, g, b, a);

	glBindTexture(GL_TEXTURE_2D, textureID);

	int tx = tileID % 8;
	int ty = tileID / 8;

	float u1 = (32*tx) / 256.0f;
	float v1 = ((32*ty) / ssh);

	float u2 = ((32*tx) + 32.0f) / 256.0f;
	float v2 = ((32*ty + 32.0f) / ssh);

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

	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

void drawBg(int x, int y, int w, int h, unsigned int textureID) {
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, textureID);

	glBegin(GL_QUADS);
	glTexCoord2f(0, 0);
	glVertex2f(x, y);
	glTexCoord2f(1, 0);
	glVertex2f(x + w, y);
	glTexCoord2f(1, 1);
	glVertex2f(x + w, y + h);
	glTexCoord2f(0, 1);
	glVertex2f(x, y + h);
	
	glEnd();
	glDisable(GL_TEXTURE_2D);

}

void overlay(float x, float y, float w, float h) {
	glColor4f(0.0f, 0.0f, 0.0f, 0.5f);

	glBegin(GL_QUADS);
	glVertex2f(x, y);
	glVertex2f(x+w, y);
	glVertex2f(x+w, y+h);
	glVertex2f(x, y+h);
	glEnd();
	
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

void drawGUI(int x1, int y1, int w, int h, unsigned int textureID, float ssh) {
	int x2 = x1+32;
	int y2 = y1+32;
	int x3 = x1+w-32;
	int y3 = y1+h-32;
	int w2 = w-64;
	int h2 = h-64;

	glColor4f(1.0f, 1.0f, 1.0f, 0.7f);
	
	drawImg(x1, y1, 32, 32, textureID, 0, ssh);
	drawImg(x2, y1, w2, 32, textureID, 1, ssh);
	drawImg(x3, y1, 32, 32, textureID, 2, ssh);
	drawImg(x1, y2, 32, h2, textureID, 3, ssh);
	drawImg(x2, y2, w2, h2, textureID, 4, ssh);
	drawImg(x3, y2, 32, h2, textureID, 5, ssh);
	drawImg(x1, y3, 32, 32, textureID, 6, ssh);
	drawImg(x2, y3, w2, 32, textureID, 7, ssh);
	drawImg(x3, y3, 32, 32, textureID, 8, ssh);
	
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

void write(int x, int y, int cpl, const char* text, unsigned int textureID, float ssh, float r = 0.0f, float g = 0.0f, float b = 0.0f, float a = 1.0f) {
	int cx = 0;
	int cy = 0;
	for (int i=0; text[i] != '\0'; i++) {
		cx += 1;
		if (cx == cpl) {
			cx = 0;
			cy += 1;
		}
		if ((int)text[i] != 32) {
			drawChar(x+(cx*32), y+(cy*32), 32, 32, textureID, (int)text[i]-44, ssh, r, g, b, a);
		}
	}
}

// we live in a cruel world

void rect(float x, float y, float w, float h) {
	glColor4f(0.0f, 0.0f, 0.0f, 1.0f);

	glBegin(GL_QUADS);
	glVertex2f(x, y);
	glVertex2f(x+w, y);
	glVertex2f(x+w, y+h);
	glVertex2f(x, y+h);
	glEnd();
	
	glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
}

int main(int argc, char* argv[]) {
	glfwInit();
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	GLFWwindow* screen = glfwCreateWindow(800, 600, "Almost There", NULL, NULL);
	glfwMakeContextCurrent(screen);
	gladLoadGL();

	glClearColor(1.0, 1.0, 1.0, 1.0);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_CULL_FACE);

	ALCdevice* device = alcOpenDevice(NULL);
	ALCcontext* context;
	if (!device) {} else {
		context = alcCreateContext(device, NULL);
		alcMakeContextCurrent(context);
	}
	std::ifstream file("assets/Namelessness.wav", std::ios::binary | std::ios::ate);
	std::streamsize asize = file.tellg();
	char* abuf = (char*)malloc(asize * sizeof(char));
	file.seekg(0, std::ios::beg);
	file.read(abuf, asize);

	ALuint abufID;
	ALuint asrcID;
	if (!device) {} else {
		alGenBuffers(1, &abufID);
		alBufferData(abufID, AL_FORMAT_STEREO16, abuf, asize * sizeof(char), 44100);

		alGenSources(1, &asrcID);
		alSourcei(asrcID, AL_BUFFER, abufID);
		alSourcei(asrcID, AL_LOOPING, AL_TRUE);
		alSourcePlay(asrcID);
	}

	int mvmnt[] = {GLFW_KEY_W, GLFW_KEY_S, GLFW_KEY_A, GLFW_KEY_D, GLFW_KEY_ESCAPE, GLFW_KEY_E, GLFW_KEY_ENTER};
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
	unsigned int uiID2 = loadTexture("assets/ui2.png");
	unsigned int fontID = loadTexture("assets/font.png");
	unsigned int bgID = loadTexture("assets/bg.png");
	unsigned int logoID = loadTexture("assets/logo1.png");

	loadprog();
	double elapsed = glfwGetTime();

	// Introducing the newest technology: The pain creator! wait no, sorry, the real name was C++.
	
	while (!glfwWindowShouldClose(screen)) {
		float fst = glfwGetTime();

		int fbw, fbh;
		glfwGetFramebufferSize(screen, &fbw, &fbh);
		glViewport(0, 0, fbw, fbh);
		
		glfwGetWindowSize(screen, &width, &height);

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0, width, height, 0, -1, 1);

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		double curx;
		double cury;
		glfwGetCursorPos(screen, &curx, &cury);

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

		holding = false;
		clickhold = false;
		if (glfwGetMouseButton(screen, GLFW_MOUSE_BUTTON_LEFT)) {
			if (!isholding) {
				holding = true;
				isholding = true;
			}
			clickhold = true;
		} else {
			isholding = false;
		}
		int furniture = -1;
		if ((int)floor(py) < specialmap.size()) {
			if ((int)floor(px) < specialmap[(int)floor(py)].size()) {
				furniture = specialmap[(int)floor(py)][(int)floor(px)-1];
			}
		}
		if (glfwGetKey(screen, mvmnt[5]) == GLFW_PRESS) {
			if (!isingui && (furniture != -1 && furniture != 12)) {
				ingui = !(ingui);
				isingui = true;
			}
		} else {
			isingui = false;
		}
		
		// WHY WHY WHY
		
		if (!(paused || ingui)) {
			float newx = px+dx;
			float newy = py+dy;
			if (wallmap[py+1][(int)floor(newx)] == -1 || wallmap[py+1][(int)floor(newx)] == 12) {
				px = newx;
			} 
			if (wallmap[(int)floor(newy)+1][px] == -1 || wallmap[(int)floor(newy)+1][px] == 12) { // why the fuck does adding +1 work
				py = newy;
			}
		} else if (paused) {
			ingui = false;
			isingui = false;
		}

		if (!device) {} else {
			alSourcef(asrcID, AL_GAIN, volume/100.0f);
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
		int pssr = 1;
		getscrxy((float)camx, (float)camy, &csx, &csy);
		if (inprol) {
			double t = glfwGetTime();
			int bgw, bgh, bgx, bgy;
			if ((float)width/(float)height <= 16.0f/9.0f) {
				bgh = height;
				bgw = 16.0f/9.0f * height;
				bgy = 0;
				bgx = (width - bgw) / 2.0f;
			} else {
				bgh = 9.0f/16.0f * width;
				bgw = width;
				bgy = (height - bgh) / 2.0f;
				bgx = 0;
			}

			drawBg(bgx, bgy, bgw, bgh, bgID);
			int shr;
			if (width < height) {
				shr = width;
			} else {
				shr = height;
			}

			drawImg((width-shr)/2, (height-shr)/2, shr, shr, playerID, 0, (float)(32*pssr));

			drawGUI(0, height-192, width, 192, uiID, 64.0f);
			drawImg(0, height-192, 192, 192, playerID, 2, (float)(32*pssr));
			write(192, (height)-160, 22, "Raine Rosalyn Renner:", fontID, 320.0f);
			if (t < 6.0) {
				write(192, (height)-96, 27, "On the 30th of July, 2026,", fontID, 320.0f);
			
			} else if (t < 10.0) {
				write(192, (height)-96, 28, "A meteor will strike Earth.", fontID, 320.0f);

			} else if (t < 14.0) {
				write(192, (height)-96, 26, "I want to visit my mother one last time.", fontID, 320.0f);
			
			} else if (t < 18.0) {
				write(192, (height)-96, 19, "Before we perish..", fontID, 320.0f);

			} else if (t < 22.0) {
				write(192, (height)-96, 28, "Her grave sits in a tightly secured dimension...", fontID, 320.0f);
			
			} else if (t < 26.0) {
				write(192, (height)-96, 33, "Player, would you help me do it?", fontID, 320.0f);

			} else {
				inmenu = true;
				inprol = false;
				glClearColor(0.0f, 0.0f, 0.1f, 1.0f);
			}

			if (glfwGetKey(screen, mvmnt[6]) == GLFW_PRESS) {
				inmenu = true;
				inprol = false;
				glClearColor(0.0f, 0.0f, 0.1f, 1.0f);
			}
		}

		else if (won) {
			double t = glfwGetTime() - timewon;
			int bgw, bgh, bgx, bgy;
			if ((float)width/(float)height <= 16.0f/9.0f) {
				bgh = height;
				bgw = 16.0f/9.0f * height;
				bgy = 0;
				bgx = (width - bgw) / 2.0f;
			} else {
				bgh = 9.0f/16.0f * width;
				bgw = width;
				bgy = (height - bgh) / 2.0f;
				bgx = 0;
			}

			drawBg(bgx, bgy, bgw, bgh, bgID);
			int shr;
			if (width < height) {
				shr = width;
			} else {
				shr = height;
			}

			drawImg((width/shr)/2, (height-shr)/2, shr, shr, playerID, 1, (float)(32*pssr));
			drawGUI(0, height-192, width, 192, uiID, 64.0f);
			drawImg(0, height-192, 192, 192, playerID, 2, (float)(32*pssr));
			write(192, (height)-160, 22, "Raine Rosalyn Renner:", fontID, 320.0f);
			if (t >= 2.0 && t < 6.0) {
				write(192, (height)-96, 64, "... [HOLD ENTER TO FAST FORWARD]", fontID, 320.0f);

			} else if (t < 2.0) {
				// does nothing

			} else if (t < 10.0) {
				write(192, (height)-96, 28, "Hello, mother....", fontID, 320.0f);

			} else if (t < 14.0) {
				write(192, (height)-96, 28, "Long time no see.", fontID, 320.0f);

			} else if (t < 18.0) {
				write(192, (height)-96, 28, "In mere minutes, the meteor will wipe us all out.", fontID, 320.0f);

			} else if (t < 22.0) {
				write(192, (height)-96, 36, "I used to believe my life wouldn't matter after the impact.", fontID, 320.0f);

			} else if (t < 26.0) {
				write(192, (height)-96, 37, "But now, I've realised.", fontID, 320.0f);

			} else if (t < 30.0) {
				write(192, (height)-96, 37, "Whether I panic or not,", fontID, 320.0f);

			} else if (t < 34.0) {
				write(192, (height)-96, 37, "It will come anyways.", fontID, 320.0f);

			} else if (t < 38.0) {
				write(192, (height)-96, 37, "All I could do is accept my fate and cherish the time I have left..", fontID, 320.0f);

			} else if (t < 42.0) {
				write(192, (height)-96, 34, "See you in the afterlife. If one exists.", fontID, 320.0f);

			} else if (t < 46.0) {
				write(192, (height)-96, 28, "Thank you, player.", fontID, 320.0f);

			} else if (t < 50.0) {
				write(192, (height)-96, 64, "See you in the afterlife too, Player.", fontID, 320.0f);

			} else if (t < 54.0) {
				write(192, (height)-96, 23, "Cherish what you have, it's not forever.", fontID, 320.0f);

			} else if (t < 58.0) {
				write(192, (height)-96, 64, "Until then, goodbye.", fontID, 320.0f);

			} else if (t < 62.0) {
				write(192, (height)-96, 25, "(by the way, press enter to return to main menu.)", fontID, 320.0f);

			}

			if (glfwGetKey(screen, mvmnt[6]) == GLFW_PRESS) {
				if (t >= 58.5) {
					won = false;
					inmenu = true;
					glClearColor(0.0f, 0.0f, 0.1f, 1.0f);
				} else {
					timewon -= 0.05;
				}
			}
		}

		else if (dead) {
			double t = glfwGetTime();
			if ((t - elapsed) < 5.0) {
				drawGUI(0, height-192, width, 192, uiID, 64.0f);
				drawImg(0, height-192, 192, 192, playerID, 2, (float)(32*pssr));
				write(192, (height)-160, 22, "Raine Rosalyn Renner:", fontID, 320.0f);
				write(192, (height)-96, 26, "We died. Let's try again.", fontID, 320.0f);
			} else {
				inmenu = true;
				dead = false;
			}
			if (glfwGetKey(screen, mvmnt[6]) == GLFW_PRESS) {
				inmenu = true;
				dead = false;
			}
		}

		else if (inmenu) {
			int bgw, bgh, bgx, bgy;
			if ((float)width/(float)height <= 16.0f/9.0f) {
				bgh = height;
				bgw = 16.0f/9.0f * height;
				bgy = 0;
				bgx = (width - bgw) / 2.0f;
			} else {
				bgh = 9.0f/16.0f * width;
				bgw = width;
				bgy = (height - bgh) / 2.0f;
				bgx = 0;
			}

			drawBg(bgx, bgy, bgw, bgh, bgID);

			int offy = 120;
			int btn1y = (height/2)-176+offy;
			int btn2y = (height/2)-104+offy;
			int btn3y = (height/2)-32+offy;
			int btn4y = (height/2)+40+offy;
			int btn5y = (height/2)+112+offy;

			int logow, logoh, logox, logoy;
			logoh = 2.0f/3.0f*btn1y;
			logoy = btn1y/6.0f;
			logow = 19.0f/8.0f * logoh;
			logox = (width-logow)/2.0f;
			drawBg(logox, logoy, logow, logoh, logoID);

			if (finished >= 0) {
				drawGUI(128, btn1y, width-256, 64, uiID, 64.0f);
			} else {
				drawGUI(128, btn1y, width-256, 64, uiID2, 64.0f);
			}
			write((width/2)-176, btn1y+14, 10, "Chapter 1", fontID, 320.0f);

			if (finished >= 1) {
				drawGUI(128, btn2y, width-256, 64, uiID, 64.0f);
			} else {
				drawGUI(128, btn2y, width-256, 64, uiID2, 64.0f);
			}
			write((width/2)-176, btn2y+14, 10, "Chapter 2", fontID, 320.0f);

			if (finished >= 2) {
				drawGUI(128, btn3y, width-256, 64, uiID, 64.0f);
			} else {
				drawGUI(128, btn3y, width-256, 64, uiID2, 64.0f);
			}
			write((width/2)-176, btn3y+14, 10, "Chapter 3", fontID, 320.0f);

			if (finished >= 3) {
				drawGUI(128, btn4y, width-256, 64, uiID, 64.0f);
			} else {
				drawGUI(128, btn4y, width-256, 64, uiID2, 64.0f);
			}
			write((width/2)-176, btn4y+14, 10, "Chapter 4", fontID, 320.0f);

			if (finished >= 4) {
				drawGUI(128, btn5y, width-256, 64, uiID, 64.0f);
			} else {
				drawGUI(128, btn5y, width-256, 64, uiID2, 64.0f);
			}
			write((width/2)-176, btn5y+14, 10, "Chapter 5", fontID, 320.0f);

			if (holding) {
				if (curx >= 128 && curx <= width - 128  && cury >= btn1y && cury <= btn1y+64 && finished >= 0) {
					level = 0;
					inmenu = false;
			
					floormap = readmap(fms[level]);
					wallmap = readmap(wms[level]);
					specialmap = readmap(sms[level]);
					pass = randrange(0, 1000000);

					elapsed = glfwGetTime();
				}
				if (curx >= 128 && curx <= width - 128  && cury >= btn2y && cury <= btn2y+64 && finished >= 1) {
					level = 1;
					inmenu = false;
			
					floormap = readmap(fms[level]);
					wallmap = readmap(wms[level]);
					specialmap = readmap(sms[level]);
					pass = randrange(0, 1000000);

					elapsed = glfwGetTime();
				}
				if (curx >= 128 && curx <= width - 128  && cury >= btn3y && cury <= btn3y+64 && finished >= 2) {
					level = 2;
					inmenu = false;
			
					floormap = readmap(fms[level]);
					wallmap = readmap(wms[level]);
					specialmap = readmap(sms[level]);
					pass = randrange(0, 1000000);

					elapsed = glfwGetTime();
				}
				if (curx >= 128 && curx <= width - 128  && cury >= btn4y && cury <= btn4y+64 && finished >= 3) {
					level = 3;
					inmenu = false;
			
					floormap = readmap(fms[level]);
					wallmap = readmap(wms[level]);
					specialmap = readmap(sms[level]);
					pass = randrange(0, 1000000);

					elapsed = glfwGetTime();
				}
				if (curx >= 128 && curx <= width - 128  && cury >= btn5y && cury <= btn5y+64 && finished >= 4) {
					level = 4;
					inmenu = false;
			
					floormap = readmap(fms[level]);
					wallmap = readmap(wms[level]);
					specialmap = readmap(sms[level]);
					pass = randrange(0, 1000000);

					elapsed = glfwGetTime();
				}
			}
		} else {
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
					if (sy-csy-136 <= -96) {
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
					buffer.push_back(added);
				}
			}

			for (int i=0; i<buffer.size(); i++) {
				drawImg(buffer[i].x, buffer[i].y, buffer[i].w, buffer[i].h, buffer[i].text, buffer[i].tile, buffer[i].ssh);
			}

			drawImg(width/2-96, height/2-96, 192, 192, playerID, state, (float)(32*pssr));

			for (int i=0; i<buffer2.size(); i++) {
				drawImg(buffer2[i].x, buffer2[i].y, buffer2[i].w, buffer2[i].h, buffer2[i].text, buffer2[i].tile, buffer2[i].ssh);
			}

			if (level == 0) {
				write(16, height-128, 49, "WASD for movements", fontID, 320.0f, 1.0f, 1.0f, 1.0f);
				write(16, height-64, 49, "E for using, ESC to open menu", fontID, 320.0f, 1.0f, 1.0f, 1.0f);
			}

			float tme = 120.0f - (glfwGetTime() - elapsed);
			if (tme > 0) {
				char timer[4];
				snprintf(timer, 4, "%03d", (int)tme);
				if (tme > 10) {
					write(width/2-80, 16, 4, (const char*)timer, fontID, 320.0f, 1.0f, 1.0f, 1.0f, 1.0f);
				} else {
					write(width/2-80, 16, 4, (const char*)timer, fontID, 320.0f, 1.0f, 0.0f, 0.0f, 1.0f);
				}
			} else {
				dead = true;
				elapsed = glfwGetTime();
			}

			if (paused) {
				overlay(0.0f, 0.0f, (float)width, (float)height);
			
				int btn1y = (height/2)-132;
				int btn2y = (height/2)-20;
				int btn3y = (height/2)+92;
				drawGUI(128, btn1y, width-256, 96, uiID, 64.0f);
				drawGUI(128, btn2y, width-256, 96, uiID, 64.0f);
				drawGUI(128, btn3y, width-256, 96, uiID, 64.0f);

				float volx = volume * ((float)(width-160) - 128) / 100;
				overlay(128+volx, btn2y, 32, 96);

				write((width/2)-224, btn1y+28, 13, "Back to Game", fontID, 320.0f);
				write((width/2)-128, btn2y+28, 7, "Volume", fontID, 320.0f);
				write((width/2)-176, btn3y+28, 10, "Main Menu", fontID, 320.0f);
			
				if (holding) {
					if (curx >= 128 && curx <= width - 128  && cury >= btn1y && cury <= btn1y+96) {
						paused = false;
					}
					if (curx >= 128 && curx <= width - 128  && cury >= btn3y && cury <= btn3y+96) {
						px = 3.0f;
						py = 3.0f;
						inmenu = true;
						paused = false;
					}
				}
				if (clickhold) {
					if (curx >= 128 && cury >= btn2y && cury <= btn2y+96) {
						volume = (curx - 128.0f) * 100.0f / (width - 288);
						if (volume > 100) volume = 100;
						else if (volume < 0) volume = 0;
					}
				}
			}

			if (ingui) {
				overlay(0.0f, 0.0f, (float)width, (float)height);
				if (furniture == 11) {
					drawGUI(width/2 - (0.3536*(height-256)), 128, 0.707*(height-256), height-256, uiID2, 64.0f);
					char buffer[7];
					snprintf(buffer, sizeof(buffer), "%d", pass);
					write(width/2 - 128, height/2 - 16, 7, buffer, fontID, 320.0f);
				} else if (furniture == 16 ) {
					won = true;
					timewon = glfwGetTime();

				} else if (furniture == 14) {
					drawGUI(width/2 - (0.3536*(height-256)), 128, 0.707*(height-256), height-256, uiID, 64.0f);
					
					float spacex1 = width/2 - (0.3536*(height-256)) + 32;
					float spacex4 = 0.707 * (height-256) + spacex1 - 64;
					float w = (spacex4 - spacex1)/3;
					float h = w*3/4;

					float spacex2 = spacex1 + w;
					float spacex3 = spacex2 + w;
					float spacey4 = height - 160;
					float spacey3 = spacey4 - h;
					float spacey2 = spacey3 - h;
					float spacey1 = spacey2 - h;
					float spacey0 = spacey1 - h;

					drawGUI(spacex1, spacey0, w, h, uiID2, 64.0f);
					drawGUI(spacex2, spacey0, w, h, uiID2, 64.0f);
					drawGUI(spacex3, spacey0, w, h, uiID2, 64.0f);

					drawGUI(spacex1, spacey1, w, h, uiID2, 64.0f);
					drawGUI(spacex2, spacey1, w, h, uiID2, 64.0f);
					drawGUI(spacex3, spacey1, w, h, uiID2, 64.0f);

					drawGUI(spacex1, spacey2, w, h, uiID2, 64.0f);
					drawGUI(spacex2, spacey2, w, h, uiID2, 64.0f);
					drawGUI(spacex3, spacey2, w, h, uiID2, 64.0f);

					drawGUI(spacex1, spacey3, w, h, uiID2, 64.0f);
					drawGUI(spacex2, spacey3, 2*w, h, uiID2, 64.0f);

					drawGUI(spacex1, spacey0-h, 3*w, h, uiID2, 64.0f);

					int xtra = w/2-48;
					int xtra2 = h/2-24;

					write(spacex1+xtra, spacey0+xtra2, 2, "7", fontID, 320.0f);
					write(spacex2+xtra, spacey0+xtra2, 2, "8", fontID, 320.0f);
					write(spacex3+xtra, spacey0+xtra2, 2, "9", fontID, 320.0f);

					write(spacex1+xtra, spacey1+xtra2, 2, "4", fontID, 320.0f);
					write(spacex2+xtra, spacey1+xtra2, 2, "5", fontID, 320.0f);
					write(spacex3+xtra, spacey1+xtra2, 2, "6", fontID, 320.0f);

					write(spacex1+xtra, spacey2+xtra2, 2, "1", fontID, 320.0f);
					write(spacex2+xtra, spacey2+xtra2, 2, "2", fontID, 320.0f);
					write(spacex3+xtra, spacey2+xtra2, 2, "3", fontID, 320.0f);

					write(spacex1+xtra, spacey3+xtra2, 2, "0", fontID, 320.0f);
					write(spacex2+w-108, spacey3+xtra2, 6, "ENTER", fontID, 320.0f);

					write(spacex1+xtra, spacey0 - 64, 7, guess, fontID, 320.0f);

					if (holding) {
						if (curx >= spacex1 && curx < spacex2 && cury >= spacey0 && cury < spacey1) {
							guess[0] = guess[1];
							guess[1] = guess[2];
							guess[2] = guess[3];
							guess[3] = guess[4];
							guess[4] = guess[5];
							guess[5] = '7';
						}

						else if (curx >= spacex2 && curx < spacex3 && cury >= spacey0 && cury < spacey1) {
							guess[0] = guess[1];
							guess[1] = guess[2];
							guess[2] = guess[3];
							guess[3] = guess[4];
							guess[4] = guess[5];
							guess[5] = '8';
						}

						else if (curx >= spacex3 && curx < spacex4 && cury >= spacey0 && cury < spacey1) {
							guess[0] = guess[1];
							guess[1] = guess[2];
							guess[2] = guess[3];
							guess[3] = guess[4];
							guess[4] = guess[5];
							guess[5] = '9';
						}

						else if (curx >= spacex1 && curx < spacex2 && cury >= spacey1 && cury < spacey2) {
							guess[0] = guess[1];
							guess[1] = guess[2];
							guess[2] = guess[3];
							guess[3] = guess[4];
							guess[4] = guess[5];
							guess[5] = '4';
						}

						else if (curx >= spacex2 && curx < spacex3 && cury >= spacey1 && cury < spacey2) {
							guess[0] = guess[1];
							guess[1] = guess[2];
							guess[2] = guess[3];
							guess[3] = guess[4];
							guess[4] = guess[5];
							guess[5] = '5';
						}

						else if (curx >= spacex3 && curx < spacex4 && cury >= spacey1 && cury < spacey2) {
							guess[0] = guess[1];
							guess[1] = guess[2];
							guess[2] = guess[3];
							guess[3] = guess[4];
							guess[4] = guess[5];
							guess[5] = '6';
						}

						else if (curx >= spacex1 && curx < spacex2 && cury >= spacey2 && cury < spacey3) {
							guess[0] = guess[1];
							guess[1] = guess[2];
							guess[2] = guess[3];
							guess[3] = guess[4];
							guess[4] = guess[5];
							guess[5] = '1';
						}

						else if (curx >= spacex2 && curx < spacex3 && cury >= spacey2 && cury < spacey3) {
							guess[0] = guess[1];
							guess[1] = guess[2];
							guess[2] = guess[3];
							guess[3] = guess[4];
							guess[4] = guess[5];
							guess[5] = '2';
						}

						else if (curx >= spacex3 && curx < spacex4 && cury >= spacey2 && cury < spacey3) {
							guess[0] = guess[1];
							guess[1] = guess[2];
							guess[2] = guess[3];
							guess[3] = guess[4];
							guess[4] = guess[5];
							guess[5] = '3';
						}

						else if (curx >= spacex1 && curx < spacex2 && cury >= spacey3 && cury < spacey4) {
							guess[0] = guess[1];
							guess[1] = guess[2];
							guess[2] = guess[3];
							guess[3] = guess[4];
							guess[4] = guess[5];
							guess[5] = '0';
						}

						else if (curx >= spacex2 && curx < spacex4 && cury >= spacey3 && cury < spacey4) {
							if (std::stoi(guess) == pass) {
								if (finished <= 4) finished += 1;
								saveprog();

								px = 3.0f;
								py = 3.0f;
								inmenu = true;
								paused = false;
								ispaused = false;
								ingui = false;
								isingui = false;
							}
						}
					}
				}
			}
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
	if (!device) {} else {
		alSourceStop(asrcID);
		alDeleteSources(1, &asrcID);
		alDeleteBuffers(1, &abufID);
	
		alcMakeContextCurrent(NULL);
		alcDestroyContext(context);
		alcCloseDevice(device);
	}
	free(abuf);
}

