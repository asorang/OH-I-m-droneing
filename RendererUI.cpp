#include <GL/glut.h>
#include <cmath>
#include <cstdio>
#include <string>
#include "RendererUI.h"

constexpr float PI = 3.14159265f;
// 카메라가 바라볼 공중 무대 중심 높이 (바닥 Y=0과 상공 Y=45의 중간 지점)
constexpr float LOOK_AT_Y = 32.0f;

// 화면에 비트맵 문자열을 찍어주는 내부 헬퍼 함수
static void RenderBitmapString(float x, float y, void* font, const std::string& str) {
    glRasterPos2f(x, y);
    for (char c : str) {
        glutBitmapCharacter(font, c);
    }
}

// 1단계: OpenGL 초기 그래픽 환경 세팅
void InitGraphics() {
    // 짙은 밤하늘 배경색 (완전한 0.0 검은색보다 살짝 남색을 섞어야 입체감이 살아남)
    glClearColor(0.02f, 0.02f, 0.07f, 1.0f);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // 점(Point)을 사각형이 아닌 둥근 원형으로 렌더링
    glEnable(GL_POINT_SMOOTH);
    glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
}

// 2단계: 3D 밤하늘 무대, 바닥 격자, LED 발광 드론 렌더링
void RenderScene(const std::vector<Drone>& drones, float camAngleX, float camAngleY, float camDist) {
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // 상하 카메라 각도 제한 (하늘 꼭대기에서 화면이 뒤집히는 짐벌락/반전 방지)
    if (camAngleY > 85.0f) camAngleY = 85.0f;
    if (camAngleY < -10.0f) camAngleY = -10.0f;

    // 구면 좌표계를 이용한 공전(Orbit) 카메라 위치 계산
    float radX = camAngleX * PI / 180.0f;
    float radY = camAngleY * PI / 180.0f;

    float camX = camDist * std::cos(radY) * std::sin(radX);
    float camY = LOOK_AT_Y + camDist * std::sin(radY);
    float camZ = camDist * std::cos(radY) * std::cos(radX);

    gluLookAt(camX, camY, camZ,          // 카메라 위치
        0.0f, LOOK_AT_Y, 0.0f,     // 카메라가 바라보는 지점
        0.0f, 1.0f, 0.0f);         // 업 벡터

    // 2-1. 바닥 이륙장 기준 격자 (드론이 땅에서 떠오르는 깊이감을 체감하기 위한 장치)
    glColor4f(0.15f, 0.2f, 0.35f, 0.4f);
    glBegin(GL_LINES);
    for (int i = -60; i <= 60; i += 10) {
        glVertex3f((float)i, -0.5f, -60.0f);
        glVertex3f((float)i, -0.5f, 60.0f);
        glVertex3f(-60.0f, -0.5f, (float)i);
        glVertex3f(60.0f, -0.5f, (float)i);
    }
    glEnd();

    // 2-2. 드론 본체(코어 LED) 렌더링
    for (const auto& d : drones) {
        glPushMatrix();
        glTranslatef(d.curX, d.curY, d.curZ);
        glColor3f(d.r, d.g, d.b);
        glutSolidSphere(0.45, 8, 8); // 500대 기준 프레임 방어를 위해 폴리곤 수 절제
        glPopMatrix();
    }

    // 2-3. LED 빛 번짐(Glow/Bloom) 후광 효과 (가산 혼합 모드 활성화)
    glDepthMask(GL_FALSE); // 빛 번짐끼리 서로 가리지 않도록 깊이 버퍼 쓰기 일시 정지
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glPointSize(14.0f);

    glBegin(GL_POINTS);
    for (const auto& d : drones) {
        // 코어 색상보다 은은한 투명도로 주변을 밝혀줌
        glColor4f(d.r, d.g, d.b, 0.35f);
        glVertex3f(d.curX, d.curY, d.curZ);
    }
    glEnd();

    // 렌더링 상태 원상복구 (안 하면 다음 프레임 화면 전체가 하얗게 타버림)
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_TRUE);
}

// 3단계: 좌상단 2D 상태창(HUD) 출력
void DrawHUD(const MemeSlot slots[], int activeSlotIndex) {
    int winW = glutGet(GLUT_WINDOW_WIDTH);
    int winH = glutGet(GLUT_WINDOW_HEIGHT);
    if (winW <= 0 || winH <= 0) return;

    // 3D 투영 행렬 백업 후 2D 직교 투영(Ortho)으로 전환
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, winW, 0, winH);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);

    // 상단 타이틀 및 조작법 안내
    glColor3f(0.4f, 0.85f, 1.0f);
    RenderBitmapString(20.0f, winH - 30.0f, GLUT_BITMAP_HELVETICA_18, "INTERNET MEME HISTORY - DRONE SHOW");

    glColor3f(0.75f, 0.75f, 0.75f);
    RenderBitmapString(20.0f, winH - 55.0f, GLUT_BITMAP_HELVETICA_12,
        "[1~6] Play Meme Slot   |   [Right-Click] Load / Replace Image   |   [Left-Drag] Rotate Camera");

    // 1~6번 슬롯 상태 리스트 출력
    char lineBuf[256];
    for (int i = 0; i < MAX_SLOTS; ++i) {
        float yPos = winH - 90.0f - (i * 22.0f);

        if (i == activeSlotIndex) {
            // 현재 재생 중인 슬롯은 눈에 띄는 노란색 하이라이트
            glColor3f(1.0f, 0.9f, 0.2f);
            std::snprintf(lineBuf, sizeof(lineBuf), ">> Slot %d : %s [PLAYING]", i + 1, slots[i].fileName.c_str());
        }
        else if (slots[i].isLoaded) {
            // 이미지는 로드되었으나 대기 중인 슬롯은 흰색
            glColor3f(0.95f, 0.95f, 0.95f);
            std::snprintf(lineBuf, sizeof(lineBuf), "   Slot %d : %s", i + 1, slots[i].fileName.c_str());
        }
        else {
            // 비어 있는 슬롯은 어두운 회색
            glColor3f(0.45f, 0.45f, 0.5f);
            std::snprintf(lineBuf, sizeof(lineBuf), "   Slot %d : (Empty - Right-click to load)", i + 1);
        }

        RenderBitmapString(20.0f, yPos, GLUT_BITMAP_9_BY_15, lineBuf);
    }

    // 3D 렌더링 환경으로 복구
    glEnable(GL_DEPTH_TEST);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

// 4단계: 마우스 우클릭 팝업 메뉴 바인딩
void SetupPopupMenu(void (*menuCallback)(int)) {
    glutCreateMenu(menuCallback);

    glutAddMenuEntry("Load / Replace Slot 1", 0);
    glutAddMenuEntry("Load / Replace Slot 2", 1);
    glutAddMenuEntry("Load / Replace Slot 3", 2);
    glutAddMenuEntry("Load / Replace Slot 4", 3);
    glutAddMenuEntry("Load / Replace Slot 5", 4);
    glutAddMenuEntry("Load / Replace Slot 6", 5);

    glutAttachMenu(GLUT_RIGHT_BUTTON);
}