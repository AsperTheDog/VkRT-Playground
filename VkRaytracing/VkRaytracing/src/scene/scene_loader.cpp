#include "scene_loader.hpp"

#include "utils/logger.hpp"

bool Scene::InitializeManager()
{
	Logger::print("Initializing FBX Manager", Logger::INFO);
    Logger::pushContext("FBX Manager Init");

	// Create manager
	s_Manager = FbxManager::Create();
	if (!s_Manager) return false;

	// Create an IOSettings object
	FbxIOSettings* l_IOSettings = FbxIOSettings::Create(s_Manager, IOSROOT);
	s_Manager->SetIOSettings(l_IOSettings);

	int l_SDK_Major, l_SDK_Minor, l_SDK_Revision;
	FbxManager::GetFileFormatVersion(l_SDK_Major, l_SDK_Minor, l_SDK_Revision);

    Logger::print((std::stringstream{} << "FBX SDK Version: " << l_SDK_Major << '.' << l_SDK_Minor << '.' << l_SDK_Revision).str(), Logger::INFO);
    Logger::popContext();
	return true;
}

void Scene::destroyManager()
{
	Logger::print("Destroying FBX Manager", Logger::INFO);
	if (s_Manager)
	{
		s_Manager->Destroy();
		s_Manager = nullptr;
	}
}

Scene::Scene(const std::filesystem::path& p_FilePath)
{
    Logger::print((std::stringstream{} << "Loading scene from file: " << p_FilePath.string()).str(), Logger::INFO);
    Logger::pushContext("Scene Load");

    if (!s_Manager)
    {
        Logger::print("FBX Manager not initialized, forcing initalization...", Logger::WARN);
        Scene::InitializeManager();
    }

    FbxImporter* l_Importer = FbxImporter::Create(s_Manager, "");
    if (!l_Importer)
    {
        Logger::print("Failed to create FBX importer", Logger::ERR);
        return;
    }
    Logger::print("FBX importer created", Logger::INFO);

    if (!l_Importer->Initialize(p_FilePath.string().c_str(), -1, s_Manager->GetIOSettings()))
    {
        Logger::print("Failed to initialize FBX importer", Logger::ERR);
        return;
    }
    Logger::print("FBX importer initialized", Logger::INFO);

    m_Scene = FbxScene::Create(s_Manager, "Scene");
    if (!m_Scene)
    {
        Logger::print("Failed to create FBX scene", Logger::ERR);
        return;
    }
    Logger::print("FBX scene created", Logger::INFO);

    if (!l_Importer->Import(m_Scene))
    {
        Logger::print("Failed to import FBX scene", Logger::ERR);
        return;
    }
    Logger::print("FBX scene imported", Logger::INFO);

    l_Importer->Destroy();
    Logger::print("FBX importer destroyed", Logger::INFO);
    Logger::print("Scene loaded successfully", Logger::INFO);
    Logger::popContext();
}
