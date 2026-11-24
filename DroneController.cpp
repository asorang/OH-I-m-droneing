#include "DroneController.h"
#include <algorithm>
#include <cmath>

// --- 물리 및 제어 상수 ---
constexpr float GRID_SPACING = 2.5f;      // 초기 대기 대형 드론 간격
constexpr float MOVE_SPEED = 3.2f;        // 목표 지점 추적 속도 (클수록 빠름)
constexpr float COLOR_SPEED = 4.0f;       // LED 색상 전환 속도
constexpr float MIN_SAFE_DIST = 1.1f;     // 드론 간 최소 안전 거리 (이보다 가까워지면 밀어냄)
constexpr float REPULSION_FORCE = 8.0f;   // 충돌 회피 반발력 세기

// 1단계: 프로그램 시작 시 드론들을 바닥(Y=0) 격자 형태로 정렬 배치
void InitDrones(std::vector<Drone>& drones, int count) {
    drones.clear();
    drones.reserve(count);

    int cols = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(count))));
    int rows = static_cast<int>(std::ceil(static_cast<float>(count) / cols));

    float offsetX = (cols - 1) * GRID_SPACING * 0.5f;
    float offsetZ = (rows - 1) * GRID_SPACING * 0.5f;

    for (int i = 0; i < count; ++i) {
        int r = i / cols;
        int c = i % cols;

        Drone d;
        d.curX = c * GRID_SPACING - offsetX;
        d.curY = 0.0f; // 이륙 전 바닥 대기 상태
        d.curZ = r * GRID_SPACING - offsetZ;

        d.targetX = d.curX;
        d.targetY = d.curY;
        d.targetZ = d.curZ;

        // 대기 상태 LED 색상 (은은한 하늘색)
        d.r = d.targetR = 0.2f;
        d.g = d.targetG = 0.6f;
        d.b = d.targetB = 1.0f;

        drones.push_back(d);
    }
}

// 2단계: X/Y 좌표 정렬을 통한 1:1 최단 동선 할당
void AssignTargets(std::vector<Drone>& drones, const MemeSlot& slot) {
    if (!slot.isLoaded || slot.points.size() != drones.size()) return;

    // 캐싱된 원본 슬롯 데이터를 보호하기 위해 복사본을 만들어 정렬
    std::vector<DroneTarget> sortedTargets = slot.points;

    // X좌표가 비슷한 열(Column)끼리 묶은 뒤 Y좌표(아래->위) 순으로 정렬하는 비교 기준
    // (단순 float X 비교 시 미세한 오차로 위아래 순서가 뒤집히는 현상 방지)
    constexpr float COLUMN_BUCKET = 2.0f;

    std::sort(drones.begin(), drones.end(), [](const Drone& a, const Drone& b) {
        int colA = static_cast<int>(std::floor(a.curX / COLUMN_BUCKET));
        int colB = static_cast<int>(std::floor(b.curX / COLUMN_BUCKET));
        if (colA != colB) return colA < colB;
        return a.curY < b.curY;
        });

    std::sort(sortedTargets.begin(), sortedTargets.end(), [](const DroneTarget& a, const DroneTarget& b) {
        int colA = static_cast<int>(std::floor(a.x / COLUMN_BUCKET));
        int colB = static_cast<int>(std::floor(b.x / COLUMN_BUCKET));
        if (colA != colB) return colA < colB;
        return a.y < b.y;
        });

    // 정렬된 순서대로 1:1 매칭 (왼쪽 아래 드론 -> 왼쪽 아래 목표로 직진)
    for (size_t i = 0; i < drones.size(); ++i) {
        drones[i].targetX = sortedTargets[i].x;
        drones[i].targetY = sortedTargets[i].y;
        drones[i].targetZ = sortedTargets[i].z;

        drones[i].targetR = sortedTargets[i].r;
        drones[i].targetG = sortedTargets[i].g;
        drones[i].targetB = sortedTargets[i].b;
    }
}

// 3단계: 매 프레임 감속 보간(Ease-Out Lerp) 이동 및 충돌 회피 연산
void UpdateDrones(std::vector<Drone>& drones, float deltaTime) {
    // 프레임 드랍이 발생해도 동일한 속도로 움직이도록 지수 감쇠 보간 계수 계산
    float posAlpha = 1.0f - std::exp(-MOVE_SPEED * deltaTime);
    float colAlpha = 1.0f - std::exp(-COLOR_SPEED * deltaTime);

    size_t n = drones.size();

    // 3-1. 목표 좌표를 향한 감속 선형 보간 (출발 시 빠르고 도착 시 부드럽게 감속)
    for (size_t i = 0; i < n; ++i) {
        drones[i].curX += (drones[i].targetX - drones[i].curX) * posAlpha;
        drones[i].curY += (drones[i].targetY - drones[i].curY) * posAlpha;
        drones[i].curZ += (drones[i].targetZ - drones[i].curZ) * posAlpha;

        drones[i].r += (drones[i].targetR - drones[i].r) * colAlpha;
        drones[i].g += (drones[i].targetG - drones[i].g) * colAlpha;
        drones[i].b += (drones[i].targetB - drones[i].b) * colAlpha;
    }

    // 3-2. 드론 간 반발력(Repulsion) 기반 충돌 회피
    // N=500 기준 약 12만 회 루프이므로 C++에서 0.2ms 이내로 연산 완료
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = i + 1; j < n; ++j) {
            float dx = drones[i].curX - drones[j].curX;
            float dy = drones[i].curY - drones[j].curY;
            float dz = drones[i].curZ - drones[j].curZ;

            float distSq = dx * dx + dy * dy + dz * dz;

            if (distSq < MIN_SAFE_DIST * MIN_SAFE_DIST) {
                float dist = std::sqrt(distSq);
                float nx, ny, nz;

                if (dist < 0.0001f) {
                    // 완전히 겹쳐서 거리가 0이 된 경우 NaN 방지를 위해 Z축 방향으로 강제 분리
                    nx = 0.0f; ny = 0.0f; nz = 1.0f;
                    dist = 0.0001f;
                }
                else {
                    // 단위 방향 벡터 정규화
                    nx = dx / dist;
                    ny = dy / dist;
                    nz = dz / dist;
                }

                // 가까울수록 강하게 밀어냄 (침투 깊이 비례)
                float overlap = (MIN_SAFE_DIST - dist) / MIN_SAFE_DIST;
                float push = overlap * REPULSION_FORCE * deltaTime;

                // 양쪽 드론을 서로 반대 방향으로 동일하게 밀어냄
                drones[i].curX += nx * push;
                drones[i].curY += ny * push;
                drones[i].curZ += nz * push;

                drones[j].curX -= nx * push;
                drones[j].curY -= ny * push;
                drones[j].curZ -= nz * push;
            }
        }
    }
}