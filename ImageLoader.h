#pragma once
#include "Types.h"

// Win32 파일 탐색기를 열어 이미지 경로를 받아옴
bool OpenImageDialog(char* outPath, int maxLen);

// 이미지를 읽어 배경을 제외하고 정확히 droneCount개만큼 샘플링해 슬롯에 저장
bool LoadMemeToSlot(MemeSlot& slot, const char* filePath, int droneCount);