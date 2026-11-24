#pragma once
#include "Types.h"
#include <vector>

// 프로그램 시작 시 드론들을 바닥 격자(대기 대형)로 초기화
void InitDrones(std::vector<Drone>& drones, int count);

// X/Y 좌표 정렬 매칭을 통해 현재 드론들에게 슬롯의 목표 좌표를 1:1 할당
void AssignTargets(std::vector<Drone>& drones, const MemeSlot& slot);

// 매 프레임 드론 위치 보간(Lerp) 이동 및 드론 간 충돌 회피(반발력) 연산
void UpdateDrones(std::vector<Drone>& drones, float deltaTime);