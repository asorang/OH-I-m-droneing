#pragma once
#include "Types.h"
#include <vector>

// OpenGL 초기 설정 (배경색, 깊이 테스트, 조명 등)
void InitGraphics();

// 3D 공간에 드론 기체 및 LED 발광 렌더링
void RenderScene(const std::vector<Drone>& drones, float camAngleX, float camAngleY, float camDist);

// 화면 좌상단에 현재 슬롯 상태 및 단축키 안내 텍스트 출력
void DrawHUD(const MemeSlot slots[], int activeSlotIndex);

// 우클릭 팝업 메뉴 생성 (콜백 함수를 인자로 전달받음)
void SetupPopupMenu(void (*menuCallback)(int));