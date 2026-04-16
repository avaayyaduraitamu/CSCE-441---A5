
#include <iostream>
#include <vector>
#include <memory>
#include <random>
#include <fstream>
#include <climits>
#include <cfloat>
#include <algorithm>
#include <string>


#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define _USE_MATH_DEFINES
#include <cmath>

#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Camera.h"
#include "GLSL.h"
#include "MatrixStack.h"
#include "Program.h"
#include "Shape.h"

using namespace std;

GLFWwindow* window;
string RESOURCE_DIR = "./";
shared_ptr<Camera> camera;
shared_ptr<Program> prog; // Pass 1
shared_ptr<Program> progPass2; // Pass 2
shared_ptr<Shape> bunnyMesh, teapotMesh, cubeMesh, revMesh, sphereMesh;

GLuint gBuffer, gPos, gNor, gKe, gKd;
int renderMode = 0; // 0: Final, 1: Pos, 2: Nor, 3: Kd, 4: Ke

bool blurOn = false;

void initDeferred(int width, int height); // Function prototype
void resize_callback(GLFWwindow* window, int width, int height); // Add this too
unsigned int quadVAO = 0, quadVBO;

class GameObject {
public:
    shared_ptr<Shape> mesh;
    bool isSphere = false;
    bool isRev = false;
    glm::vec3 pos, color;
    float rotY, scale;
    GameObject(shared_ptr<Shape> m, glm::vec3 p, float r, float s, glm::vec3 c, bool sphere = false, bool rev = false)
        : mesh(m), pos(p), rotY(r), scale(s), color(c), isSphere(sphere), isRev(rev) {
    }
};

class Light {
public:
    glm::vec3 pos, col;
    Light(glm::vec3 p, glm::vec3 c) : pos(p), col(c) {}
};

vector<shared_ptr<GameObject>> worldObjects;
vector<shared_ptr<Light>> lights;

bool keyW = false, keyA = false, keyS = false, keyD = false;
bool keyZ = false, keyShiftZ = false;

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);

    bool isPressed = (action != GLFW_RELEASE);
    if (key == GLFW_KEY_W) keyW = isPressed;
    if (key == GLFW_KEY_A) keyA = isPressed;
    if (key == GLFW_KEY_S) keyS = isPressed;
    if (key == GLFW_KEY_D) keyD = isPressed;
    if (key == GLFW_KEY_Z) {
        if (mods & GLFW_MOD_SHIFT) { keyShiftZ = isPressed; keyZ = false; }
        else { keyZ = isPressed; keyShiftZ = false; }
    }

    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_0) renderMode = 0;
        else if (key == GLFW_KEY_1) renderMode = 1;
        else if (key == GLFW_KEY_2) renderMode = 2;
        else if (key == GLFW_KEY_3) renderMode = 3;
        else if (key == GLFW_KEY_4) renderMode = 4;
    }


    if (key == GLFW_KEY_B && action == GLFW_PRESS) {
        blurOn = !blurOn;
    }
}

static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos) {
    camera->mouseMoved((float)xpos, (float)ypos);
}

void resize_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
    // Re-initialize the textures to match the new window size
    initDeferred(width, height);
}



void initDeferred(int width, int height) {
    // Delete old buffers/textures if they already exist
    static bool firstTime = true;
    if (!firstTime) {
        glDeleteFramebuffers(1, &gBuffer);
        glDeleteTextures(1, &gPos); glDeleteTextures(1, &gNor);
        glDeleteTextures(1, &gKe);  glDeleteTextures(1, &gKd);
    }
    firstTime = false;

    glGenFramebuffers(1, &gBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);

    GLuint* texs[] = { &gPos, &gNor, &gKe, &gKd };
    for (int i = 0; i < 4; i++) {
        glGenTextures(1, texs[i]);
        glBindTexture(GL_TEXTURE_2D, *texs[i]);
        // Note: width and height come from the resize callback
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, *texs[i], 0);
    }

    GLuint depthBuf;
    glGenRenderbuffers(1, &depthBuf);
    glBindRenderbuffer(GL_RENDERBUFFER, depthBuf);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuf);

    unsigned int attachments[4] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
    glDrawBuffers(4, attachments);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void renderQuad() {
    if (quadVAO == 0) {
        float quadVertices[] = {
            -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
             1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
             1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        };
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    }
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

void saveSphereObj(const string& filename, int res) {
    ofstream outFile(filename);
    if (!outFile.is_open()) return;
    for (int i = 0; i <= res; i++) {
        float theta = (float)M_PI * i / res;
        for (int j = 0; j <= res; j++) {
            float phi = 2.0f * (float)M_PI * j / res;
            float x = sin(theta) * sin(phi), y = cos(theta), z = sin(theta) * cos(phi);
            outFile << "v " << x << " " << y << " " << z << "\n";
            outFile << "vn " << x << " " << y << " " << z << "\n";
        }
    }
    for (int i = 0; i < res; i++) {
        for (int j = 0; j < res; j++) {
            int v1 = i * (res + 1) + j + 1, v2 = v1 + res + 1, v3 = v1 + 1, v4 = v2 + 1;
            outFile << "f " << v1 << "//" << v1 << " " << v2 << "//" << v2 << " " << v3 << "//" << v3 << "\n";
            outFile << "f " << v3 << "//" << v3 << " " << v2 << "//" << v2 << " " << v4 << "//" << v4 << "\n";
        }
    }
    outFile.close();
}

void generateRevObj(const string& filename) {
    ofstream outFile(filename);
    if (!outFile.is_open()) return;
    int sectors = 40, layers = 40;
    for (int i = 0; i <= layers; i++) {
        float x = (i / (float)layers);
        for (int j = 0; j <= sectors; j++) {
            float theta = 2.0f * (float)M_PI * j / sectors;
            outFile << "v " << x << " " << theta << " 0.0\n";
            outFile << "vn 0 0 0\n";
        }
    }
    for (int i = 0; i < layers; i++) {
        for (int j = 0; j < sectors; j++) {
            int v1 = i * (sectors + 1) + j + 1, v2 = v1 + sectors + 1, v3 = v1 + 1, v4 = v2 + 1;
            outFile << "f " << v1 << " " << v2 << " " << v3 << "\n";
            outFile << "f " << v3 << " " << v2 << " " << v4 << "\n";
        }
    }
    outFile.close();
}

static void init(int argc, char** argv) {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glEnable(GL_DEPTH_TEST);

    saveSphereObj(RESOURCE_DIR + "sphere.obj", 50);
    generateRevObj(RESOURCE_DIR + "rev.obj");

    bunnyMesh = make_shared<Shape>(); bunnyMesh->loadMesh(RESOURCE_DIR + "bunny.obj"); bunnyMesh->init();
    teapotMesh = make_shared<Shape>(); teapotMesh->loadMesh(RESOURCE_DIR + "teapot.obj"); teapotMesh->init();
    cubeMesh = make_shared<Shape>(); cubeMesh->loadMesh(RESOURCE_DIR + "cube.obj"); cubeMesh->init();
    revMesh = make_shared<Shape>(); revMesh->loadMesh(RESOURCE_DIR + "rev.obj"); revMesh->init();
    sphereMesh = make_shared<Shape>(); sphereMesh->loadMesh(RESOURCE_DIR + "sphere.obj"); sphereMesh->init();

    prog = make_shared<Program>();
    prog->setShaderNames(RESOURCE_DIR + "bp_vert.glsl", RESOURCE_DIR + "bp_frag.glsl");
    prog->init();
    prog->addAttribute("aPos"); prog->addAttribute("aNor");
    prog->addAttribute("aTex");
    prog->addUniform("t"); prog->addUniform("P"); prog->addUniform("MV"); prog->addUniform("MVit");
    prog->addUniform("kd"); prog->addUniform("ke"); prog->addUniform("isRev");

    progPass2 = make_shared<Program>();
    progPass2->setShaderNames(RESOURCE_DIR + "pass2_vert.glsl", RESOURCE_DIR + "pass2_frag.glsl");
    progPass2->init();
    progPass2->addAttribute("aPos");
    progPass2->addUniform("posTexture"); progPass2->addUniform("norTexture");
    progPass2->addUniform("keTexture"); progPass2->addUniform("kdTexture");
    progPass2->addUniform("mode"); progPass2->addUniform("windowSize");
    progPass2->addUniform("blurOn");
    progPass2->addUniform("lightPos"); progPass2->addUniform("lightCol"); progPass2->addUniform("numLights");

    camera = make_shared<Camera>();
    camera->setInitDistance(10.0f);

    if (argc >= 3 && string(argv[2]) == "1") {
                lights.push_back(make_shared<Light>(glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f)));
            }
     else {
                for (int i = 0; i < 10; i++) {
                    lights.push_back(make_shared<Light>(
                        glm::vec3(((rand() % 100) / 10.0f) - 5, 1.0f, ((rand() % 100) / 10.0f) - 5),
                        glm::vec3((rand() % 100) / 100.0f, (rand() % 100) / 100.0f, (rand() % 100) / 100.0f)
                    ));
                }
     }
        
    float groundSize = 10.0f;
    float minDistance = 0.6f; // Adjust this to control how close objects can get

    while (worldObjects.size() < 105) {
        float x = ((rand() % 1001) / 100.0f) - 5.0f;
        float z = ((rand() % 1001) / 100.0f) - 5.0f;

        // Check if within ground bounds (taking scale into account)
        float s = 0.15f + (rand() % 100) / 500.0f;
        if (abs(x) > 4.8f || abs(z) > 4.8f) continue;

        // Intersection Check
        bool overlap = false;
        for (auto& existing : worldObjects) {
            float d = glm::distance(glm::vec2(x, z), glm::vec2(existing->pos.x, existing->pos.z));
            if (d < minDistance) {
                overlap = true;
                break;
            }
        }

        if (!overlap) {
            glm::vec3 color((rand() % 100) / 100.0f, (rand() % 100) / 100.0f, (rand() % 100) / 100.0f);
            int type = rand() % 4;
            if (type == 0) worldObjects.push_back(make_shared<GameObject>(bunnyMesh, glm::vec3(x, 0, z), (float)(rand() % 360), s, color));
            else if (type == 1) worldObjects.push_back(make_shared<GameObject>(teapotMesh, glm::vec3(x, 0, z), (float)(rand() % 360), s, color));
            else if (type == 2) worldObjects.push_back(make_shared<GameObject>(sphereMesh, glm::vec3(x, 0, z), 0, s, color, true, false));
            else worldObjects.push_back(make_shared<GameObject>(revMesh, glm::vec3(x, 0, z), 0, s * 0.5f, color, false, true));
        }
    }

    int w, h; glfwGetFramebufferSize(window, &w, &h);
    initDeferred(w, h);
}

void render() {
    camera->updateWASD(keyW, keyA, keyS, keyD);
    camera->updateZoom(keyZ, keyShiftZ);
    int width, height; glfwGetFramebufferSize(window, &width, &height);

    // --- PASS 1: GEOMETRY PASS ---
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    auto P = make_shared<MatrixStack>(), MV = make_shared<MatrixStack>();
    float t = (float)glfwGetTime();

    prog->bind();
    camera->setAspect((float)width / (float)height);
    P->pushMatrix(); camera->applyProjectionMatrix(P);
    MV->pushMatrix(); camera->applyViewMatrix(MV);

    glUniform1f(prog->getUniform("t"), t);
    glUniformMatrix4fv(prog->getUniform("P"), 1, GL_FALSE, glm::value_ptr(P->topMatrix()));

    // Ground
    MV->pushMatrix();
    MV->translate(glm::vec3(0, -0.01f, 0)); MV->scale(glm::vec3(10.0f, 0.01f, 10.0f));
    glUniformMatrix4fv(prog->getUniform("MV"), 1, GL_FALSE, glm::value_ptr(MV->topMatrix()));
    glUniformMatrix4fv(prog->getUniform("MVit"), 1, GL_FALSE, glm::value_ptr(glm::transpose(glm::inverse(MV->topMatrix()))));
    glUniform3f(prog->getUniform("kd"), 0.2f, 0.2f, 0.2f); glUniform3f(prog->getUniform("ke"), 0, 0, 0);
    glUniform1i(prog->getUniform("isRev"), 0);
    cubeMesh->draw(prog);
    MV->popMatrix();

    // Light Bulbs
    for (auto& l : lights) {
        MV->pushMatrix(); MV->translate(l->pos); MV->scale(0.05f);
        glUniformMatrix4fv(prog->getUniform("MV"), 1, GL_FALSE, glm::value_ptr(MV->topMatrix()));
        glUniformMatrix4fv(prog->getUniform("MVit"), 1, GL_FALSE, glm::value_ptr(glm::transpose(glm::inverse(MV->topMatrix()))));
        glUniform3f(prog->getUniform("kd"), 0, 0, 0); glUniform3fv(prog->getUniform("ke"), 1, glm::value_ptr(l->col));
        sphereMesh->draw(prog);
        MV->popMatrix();
    }
    glUniform3f(prog->getUniform("ke"), 0, 0, 0);

    // World Objects
    for (auto& obj : worldObjects) {
        MV->pushMatrix();
        glUniform1i(prog->getUniform("isRev"), obj->isRev ? 1 : 0);
        if (obj->isSphere) {
            float p = 2.0f, Ay = 0.5f, As = 0.5f;
            float y_trans = Ay * (0.5f * sin((2.0f * (float)M_PI / p) * t) + 0.5f);
            float s_xz = -As * (0.5f * cos((4.0f * (float)M_PI / p) * t) + 0.5f) + 1.0f;
            float s_y = 1.0f / s_xz;
            MV->translate(obj->pos.x, y_trans + (obj->scale * s_y), obj->pos.z);
            MV->scale(glm::vec3(obj->scale * s_xz, obj->scale * s_y, obj->scale * s_xz));
        }
        else if (obj->isRev) {
            MV->translate(obj->pos.x, 0, obj->pos.z);
            MV->rotate(t, glm::vec3(0, 1, 0));
            MV->scale(obj->scale);
        }
        else {
            MV->translate(obj->pos.x, 0, obj->pos.z);
            if (obj->mesh == bunnyMesh) MV->rotate(obj->rotY + t, glm::vec3(0, 1, 0));
            if (obj->mesh == teapotMesh) {
                float s = 0.3f * sin(t * 3.0f);
                glm::mat4 shear(1.0f); shear[1][0] = s;
                MV->multMatrix(shear);
            }
            MV->scale(obj->scale);
        }
        glUniformMatrix4fv(prog->getUniform("MV"), 1, GL_FALSE, glm::value_ptr(MV->topMatrix()));
        glUniformMatrix4fv(prog->getUniform("MVit"), 1, GL_FALSE, glm::value_ptr(glm::transpose(glm::inverse(MV->topMatrix()))));
        glUniform3fv(prog->getUniform("kd"), 1, glm::value_ptr(obj->color));
        obj->mesh->draw(prog);
        MV->popMatrix();
    }
    prog->unbind();

    // --- PASS 2: LIGHTING PASS ---
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    progPass2->bind();

    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, gPos); glUniform1i(progPass2->getUniform("posTexture"), 0);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, gNor); glUniform1i(progPass2->getUniform("norTexture"), 1);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, gKe);  glUniform1i(progPass2->getUniform("keTexture"), 2);
    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, gKd);  glUniform1i(progPass2->getUniform("kdTexture"), 3);

    glUniform1i(progPass2->getUniform("mode"), renderMode);
    glUniform2f(progPass2->getUniform("windowSize"), (float)width, (float)height);

    glUniform1i(progPass2->getUniform("blurOn"), blurOn ? 1 : 0);

    vector<float> lp, lc;
    MV->pushMatrix(); camera->applyViewMatrix(MV);
    for (auto& l : lights) {
        glm::vec3 camSpacePos = glm::vec3(MV->topMatrix() * glm::vec4(l->pos, 1.0f));
        lp.push_back(camSpacePos.x); lp.push_back(camSpacePos.y); lp.push_back(camSpacePos.z);
        lc.push_back(l->col.r); lc.push_back(l->col.g); lc.push_back(l->col.b);
    }
    MV->popMatrix();
    glUniform3fv(progPass2->getUniform("lightPos"), (GLsizei)lights.size(), lp.data());
    glUniform3fv(progPass2->getUniform("lightCol"), (GLsizei)lights.size(), lc.data());
    glUniform1i(progPass2->getUniform("numLights"), (int)lights.size());

    renderQuad();
    progPass2->unbind();
}

int main(int argc, char** argv) {
    if (argc >= 2) RESOURCE_DIR = argv[1] + string("/");
    if (!glfwInit()) return -1;
    window = glfwCreateWindow(640, 480, "Avanthika Ayyadurai", NULL, NULL);
    glfwMakeContextCurrent(window); glewExperimental = true; glewInit();
    glfwSetKeyCallback(window, key_callback); glfwSetCursorPosCallback(window, cursor_pos_callback);
    glfwSetFramebufferSizeCallback(window, resize_callback);
    init(argc, argv);
    while (!glfwWindowShouldClose(window)) { render(); glfwSwapBuffers(window); glfwPollEvents(); }
    glfwTerminate(); return 0;
}