#pragma once
#include <fbxsdk.h>
#include <filesystem>

class Scene
{
public:
    static bool InitializeManager();
    static void destroyManager();

    explicit Scene(const std::filesystem::path& p_FilePath);

private:
    inline static FbxManager* s_Manager = nullptr;
    FbxScene* m_Scene = nullptr;
};
