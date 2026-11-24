#pragma once
#include <vector>
#include <string>

// 프로젝트 전역 상수
constexpr int MAX_DRONES = 500;   // 총 드론 대수
constexpr int MAX_SLOTS = 6;      // 저장 가능한 최대 밈 슬롯 수 (숫자키 1~6)

// 목표 좌표 및 색상 데이터 (이미지에서 추출되는 단위)
struct DroneTarget {
    float x, y, z;
    float r, g, b;
};

// 개별 드론 객체 상태
struct Drone {
    float curX, curY, curZ;          // 현재 위치
    float targetX, targetY, targetZ; // 할당된 목표 위치
    float r, g, b;                   // 현재 LED 색상
    float targetR, targetG, targetB; // 목표 LED 색상
};

// 1~6번 슬롯에 캐싱되는 밈 데이터
struct MemeSlot {
    bool isLoaded = false;               // 이미지 로드 여부
    std::string fileName = "Empty";      // UI 표시용 파일 이름
    std::vector<DroneTarget> points;     // 추출된 목표 좌표 (크기는 항상 MAX_DRONES)
};