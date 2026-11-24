#include <GL/glew.h> // 또는 환경에 맞게 freeglut.h / glut.h 포함
#include <GL/glut.h>
#include <vector>
#include <cstdio>

#include "Types.h"
#include "ImageLoader.h"
#include "DroneController.h"
#include "RendererUI.h"

// --- 전역 상태 데이터 (main.cpp가 유일하게 소유) ---
std::vector<Drone> g_drones;
MemeSlot g_slots[MAX_SLOTS];
int g_currentSlot = -1; // 현재 활성화된 슬롯 (-1은 초기 대기 대형)

// 카메라 제어 변수
float g_camAngleX = 0.0f;
float g_camAngleY = 15.0f;
float g_camDist = 120.0f;
int g_lastMouseX = 0, g_lastMouseY = 0;
bool g_isDragging = false;

// --- 콜백 함수들 ---

// 1. 우클릭 메뉴 선택 시 호출 (1~6번 슬롯 이미지 불러오기/교체)
void OnMenuSelect(int slotIndex) {
    if (slotIndex < 0 || slotIndex >= MAX_SLOTS) return;

    char filePath[260] = { 0 };
    // 1) 윈도우 파일 탐색기 오픈
    if (OpenImageDialog(filePath, sizeof(filePath))) {
        // 2) 이미지 파싱 및 MAX_DRONES 개수로 정규화하여 슬롯에 캐싱
        if (LoadMemeToSlot(g_slots[slotIndex], filePath, MAX_DRONES)) {
            printf("[Success] Slot %d loaded: %s\n", slotIndex + 1, filePath);

            // 만약 현재 화면에 띄워진 슬롯을 교체한 거라면 즉시 드론 목표 갱신
            if (g_currentSlot == slotIndex) {
                AssignTargets(g_drones, g_slots[slotIndex]);
            }
        }
        else {
            printf("[Error] Failed to load image into Slot %d\n", slotIndex + 1);
        }
    }
}

// 2. 키보드 입력 ('1'~'6' 키로 슬롯 전환)
void OnKeyboard(unsigned char key, int x, int y) {
    if (key >= '1' && key <= '6') {
        int slotIdx = key - '1';
        if (g_slots[slotIdx].isLoaded) {
            g_currentSlot = slotIdx;
            AssignTargets(g_drones, g_slots[slotIdx]);
            printf("[Play] Switched to Slot %d\n", slotIdx + 1);
        }
        else {
            printf("[Warning] Slot %d is empty! Right-click to load an image.\n", slotIdx + 1);
        }
    }
    else if (key == 27) { // ESC 종료
        exit(0);
    }
    glutPostRedisplay();
}

// 3. 마우스 클릭 및 드래그 (카메라 공전)
void OnMouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON) {
        g_isDragging = (state == GLUT_DOWN);
        g_lastMouseX = x;
        g_lastMouseY = y;
    }
}

void OnMotion(int x, int y) {
    if (g_isDragging) {
        g_camAngleX += (x - g_lastMouseX) * 0.5f;
        g_camAngleY += (y - g_lastMouseY) * 0.5f;
        g_lastMouseX = x;
        g_lastMouseY = y;
        glutPostRedisplay();
    }
}

// 4. 타이머 콜백 (약 60FPS 물리/이동 업데이트)
void OnTimer(int value) {
    UpdateDrones(g_drones, 0.016f); // 16ms 기준 업데이트
    glutPostRedisplay();
    glutTimerFunc(16, OnTimer, 0);
}

// 5. 화면 렌더링 콜백
void OnDisplay() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    RenderScene(g_drones, g_camAngleX, g_camAngleY, g_camDist);
    DrawHUD(g_slots, g_currentSlot);

    glutSwapBuffers();
}

// 6. 창 크기 변경 대응
void OnReshape(int w, int h) {
    if (h == 0) h = 1;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (double)w / (double)h, 1.0, 1000.0);
    glMatrixMode(GL_MODELVIEW);
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(1280, 720);
    glutCreateWindow("Internet Meme History - Drone Show Simulator");

    InitGraphics();
    InitDrones(g_drones, MAX_DRONES);
    SetupPopupMenu(OnMenuSelect);

    glutDisplayFunc(OnDisplay);
    glutReshapeFunc(OnReshape);
    glutKeyboardFunc(OnKeyboard);
    glutMouseFunc(OnMouse);
    glutMotionFunc(OnMotion);
    glutTimerFunc(16, OnTimer, 0);

    glutMainLoop();
    return 0;
}