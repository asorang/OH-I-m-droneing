#define NOMINMAX
#define _CRT_SECURE_NO_WARNINGS
#define STB_IMAGE_IMPLEMENTATION

#include <windows.h>
#include <commdlg.h>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <cstdio>

#include "stb_image.h"
#include "ImageLoader.h"

// 드론 군집 밀도를 높이기 위해 기존 70.0f -> 50.0f로 압축 (이목구비 선명도 향상)
constexpr float SHOW_SCALE = 50.0f;
constexpr float SHOW_CENTER_Y = 42.0f;

// 배경 제거 및 외곽선 판별 임계값
constexpr float BG_COLOR_TOLERANCE = 48.0f;  // 좌상단 배경색과의 허용 오차 (JPEG 노이즈 방어)
constexpr float EDGE_DIFF_THRESHOLD = 75.0f; // 인접 픽셀과 이 이상 색 차이가 나면 '외곽선'으로 판정
constexpr float EDGE_DRONE_RATIO = 0.70f;    // 전체 드론 중 70%를 외곽선(윤곽/이목구비)에 우선 배정

// 1단계: 윈도우 파일 탐색기 호출 (기존과 동일)
bool OpenImageDialog(char* outPath, int maxLen) {
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ZeroMemory(outPath, maxLen);

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = outPath;
    ofn.nMaxFile = maxLen;
    ofn.lpstrFilter = "Image Files (*.png;*.jpg;*.jpeg;*.bmp)\0*.png;*.jpg;*.jpeg;*.bmp\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    return GetOpenFileNameA(&ofn) == TRUE;
}

// 픽셀이 배경인지 판별하는 내부 헬퍼 함수
static bool IsBackgroundPixel(const unsigned char* imgData, int idx, bool useColorKey,
    unsigned char bgR, unsigned char bgG, unsigned char bgB) {
    unsigned char r = imgData[idx + 0];
    unsigned char g = imgData[idx + 1];
    unsigned char b = imgData[idx + 2];
    unsigned char a = imgData[idx + 3];

    // 1) 투명 픽셀은 무조건 배경
    if (a < 128) return true;

    // 2) 불투명 이미지인 경우, 좌상단 (0, 0) 코너 색상과 비슷하면 배경으로 간주
    if (useColorKey) {
        float dr = static_cast<float>(r) - bgR;
        float dg = static_cast<float>(g) - bgG;
        float db = static_cast<float>(b) - bgB;
        float dist = std::sqrt(dr * dr + dg * dg + db * db);
        if (dist < BG_COLOR_TOLERANCE) return true;
    }

    return false;
}

// 2단계: 스마트 배경 제거 + 외곽선(Edge) 가중치 샘플링
bool LoadMemeToSlot(MemeSlot& slot, const char* filePath, int droneCount) {
    int width = 0, height = 0, channels = 0;
    unsigned char* imgData = stbi_load(filePath, &width, &height, &channels, 4);
    if (!imgData) {
        printf("[ImageLoader] 파일 로드 실패: %s\n", filePath);
        return false;
    }

    // 2-1. 좌상단 (0, 0) 픽셀을 스포이드로 찍어 기준 배경색 추출
    unsigned char bgR = imgData[0];
    unsigned char bgG = imgData[1];
    unsigned char bgB = imgData[2];
    unsigned char bgA = imgData[3];
    // (0,0)이 투명하지 않다면 해당 색상을 배경 키로 활용
    bool useColorKey = (bgA >= 128);

    struct PixelNode {
        int px, py;
        float r, g, b;
    };

    std::vector<PixelNode> edgePixels; // 외곽선 & 이목구비 픽셀 집단
    std::vector<PixelNode> fillPixels; // 내부 채우기용 픽셀 집단
    edgePixels.reserve(width * height / 4);
    fillPixels.reserve(width * height / 2);

    // 2-2. 전체 픽셀 순회하며 배경 필터링 및 인접 픽셀 색상 대비(Edge) 계산
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = (y * width + x) * 4;

            if (IsBackgroundPixel(imgData, idx, useColorKey, bgR, bgG, bgB)) {
                continue;
            }

            unsigned char r = imgData[idx + 0];
            unsigned char g = imgData[idx + 1];
            unsigned char b = imgData[idx + 2];

            bool isEdge = false;

            // 상하좌우 4방향 인접 픽셀 검사
            const int dx[4] = { 1, -1, 0, 0 };
            const int dy[4] = { 0, 0, 1, -1 };

            for (int dir = 0; dir < 4; ++dir) {
                int nx = x + dx[dir];
                int ny = y + dy[dir];

                // 이미지 테두리에 닿아있거나 인접 픽셀이 배경이면 무조건 최외곽 윤곽선!
                if (nx < 0 || nx >= width || ny < 0 || ny >= height) {
                    isEdge = true;
                    break;
                }

                int nIdx = (ny * width + nx) * 4;
                if (IsBackgroundPixel(imgData, nIdx, useColorKey, bgR, bgG, bgB)) {
                    isEdge = true;
                    break;
                }

                // 인접 유효 픽셀과의 RGB 유클리드 거리 계산 (눈, 코, 입 등 내부 경계선 포착)
                float diffR = static_cast<float>(r) - imgData[nIdx + 0];
                float diffG = static_cast<float>(g) - imgData[nIdx + 1];
                float diffB = static_cast<float>(b) - imgData[nIdx + 2];
                float colorDiff = std::sqrt(diffR * diffR + diffG * diffG + diffB * diffB);

                if (colorDiff > EDGE_DIFF_THRESHOLD) {
                    isEdge = true;
                    break;
                }
            }

            PixelNode node = { x, y, r / 255.0f, g / 255.0f, b / 255.0f };
            if (isEdge) {
                edgePixels.push_back(node);
            }
            else {
                fillPixels.push_back(node);
            }
        }
    }

    // 예외 처리: 단색 백지 등 유효 픽셀이 0개인 경우 전체 픽셀을 내부 픽셀로 취급
    if (edgePixels.empty() && fillPixels.empty()) {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int idx = (y * width + x) * 4;
                fillPixels.push_back({
                    x, y,
                    imgData[idx] / 255.0f,
                    imgData[idx + 1] / 255.0f,
                    imgData[idx + 2] / 255.0f
                    });
            }
        }
    }

    // 2-3. 드론 정원 배분 (외곽선 70% + 내부 면적 30%)
    int edgeQuota = static_cast<int>(droneCount * EDGE_DRONE_RATIO);
    int actualEdgeCount = std::min(edgeQuota, static_cast<int>(edgePixels.size()));
    int actualFillCount = droneCount - actualEdgeCount;

    // 만약 내부 픽셀이 부족하면 남은 자리를 외곽선 픽셀이 다시 채움
    if (fillPixels.empty()) {
        actualEdgeCount = droneCount;
        actualFillCount = 0;
    }

    std::vector<PixelNode> selectedPixels;
    selectedPixels.reserve(droneCount);

    // 외곽선 집단에서 균등 추출
    if (!edgePixels.empty() && actualEdgeCount > 0) {
        int totalEdges = static_cast<int>(edgePixels.size());
        for (int i = 0; i < actualEdgeCount; ++i) {
            int sampleIdx = static_cast<int>((static_cast<long long>(i) * totalEdges) / actualEdgeCount) % totalEdges;
            selectedPixels.push_back(edgePixels[sampleIdx]);
        }
    }

    // 내부 집단에서 균등 추출
    if (!fillPixels.empty() && actualFillCount > 0) {
        int totalFills = static_cast<int>(fillPixels.size());
        for (int i = 0; i < actualFillCount; ++i) {
            int sampleIdx = static_cast<int>((static_cast<long long>(i) * totalFills) / actualFillCount) % totalFills;
            selectedPixels.push_back(fillPixels[sampleIdx]);
        }
    }

    // 2-4. 3D 월드 좌표 정규화 및 미세 분산(Jitter) 적용
    float maxDim = static_cast<float>(std::max(width, height));
    float halfW = width * 0.5f;
    float halfH = height * 0.5f;

    slot.points.clear();
    slot.points.reserve(droneCount);

    int totalUnique = static_cast<int>(edgePixels.size() + fillPixels.size());

    for (int i = 0; i < droneCount; ++i) {
        const PixelNode& p = selectedPixels[i];

        DroneTarget target;
        target.x = ((p.px - halfW) / maxDim) * SHOW_SCALE;
        target.y = (-(p.py - halfH) / maxDim) * SHOW_SCALE + SHOW_CENTER_Y;
        target.z = 0.0f;

        // 저해상도 도트 이미지여서 픽셀이 중복 할당될 경우 겹침 방지
        if (totalUnique < droneCount) {
            float jitterAngle = i * 2.39996f;
            target.x += std::cos(jitterAngle) * 0.25f;
            target.y += std::sin(jitterAngle) * 0.25f;
            target.z += ((i % 3) - 1) * 0.2f;
        }

        target.r = p.r;
        target.g = p.g;
        target.b = p.b;

        slot.points.push_back(target);
    }

    stbi_image_free(imgData);

    std::string fullPath(filePath);
    size_t lastSlash = fullPath.find_last_of("\\/");
    slot.fileName = (lastSlash != std::string::npos) ? fullPath.substr(lastSlash + 1) : fullPath;
    slot.isLoaded = true;

    printf("[ImageLoader] %s 로드 완료 (외곽선 드론: %d대, 내부 채움 드론: %d대)\n",
        slot.fileName.c_str(), actualEdgeCount, actualFillCount);

    return true;
}