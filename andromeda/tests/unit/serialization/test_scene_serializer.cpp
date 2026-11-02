// Scene files: components come back as saved, mesh references survive renames (Guid first, name as
// fallback), and a damaged file never leaves the editor with half a scene.

#include <gtest/gtest.h>
#include <algorithm>
#include <optional>
#include <unordered_map>
#include <vector>
#include "sceneSerializer.hpp"
#include "a_LogCapture.hpp"
#include "a_TempDirectory.hpp"

using namespace Andromeda;
using namespace Andromeda::ECS::Component;
using Andromeda::Test::LogCapture;
using Andromeda::Test::TempDirectory;

namespace {
    /** @brief Answers the serializer's mesh questions from two maps instead of loaded models. */
    class FakeMeshLookup final : public IMeshLookup {
    public:
        void addMesh(u32 id, const std::string& name, std::optional<Guid> guid = std::nullopt) {
            m_Names[id] = name;
            if (guid) {
                m_Guids[id] = *guid;
            }
        }

        std::string getMeshNameByID(u32 id) const override {
            const auto it = m_Names.find(id);
            return it != m_Names.end() ? it->second : std::string{};
        }
        bool tryGetMeshIDByName(const std::string& name, u32& out) const override {
            const auto it = std::ranges::find_if(m_Names, [&name](const auto& entry) { return entry.second == name; });
            if (it == m_Names.end()) return false;
            out = it->first;
            return true;
        }
        bool tryGetMeshIDByGuid(const Guid& guid, u32& out) const override {
            const auto it = std::ranges::find_if(m_Guids, [&guid](const auto& entry) { return entry.second == guid; });
            if (it == m_Guids.end()) return false;
            out = it->first;
            return true;
        }
        bool tryGetGuidByMeshID(u32 meshID, Guid& out) const override {
            const auto it = m_Guids.find(meshID);
            if (it == m_Guids.end()) return false;
            out = it->second;
            return true;
        }

    private:
        std::unordered_map<u32, std::string> m_Names;
        std::unordered_map<u32, Guid> m_Guids;
    };

    Guid guid(const char* text) {
        return *Guid::fromString(text);
    }

    const Guid apoGuid = guid("969c6c10ad374b528d49ea4cab8b5fce");
    const std::string meshKey = typeid(MeshRenderer).name();

    template<typename T>
    nlohmann::json poolJson(ECS::ComponentRegistry& registry) {
        return registry.getPool<T>().serializePool();
    }

    std::vector<ECS::Entity> sortedEntities(const ECS::ComponentRegistry& registry) {
        std::vector<ECS::Entity> entities = registry.getAllEntities();
        std::ranges::sort(entities);
        return entities;
    }
}

class SceneSerializerTest : public ::testing::Test {
protected:
    FakeMeshLookup m_Meshes;
    ECS::ComponentRegistry m_Scene;

    void SetUp() override {
        // Mesh 0 on purpose: the first loaded mesh has ID 0, which once got mistaken for "no mesh".
        m_Meshes.addMesh(0, "apo", apoGuid);
        m_Meshes.addMesh(7, "Default_Cube");
    }

    ECS::Entity addMeshEntity(u32 meshID) {
        const ECS::Entity entity = m_Scene.createEntity();
        m_Scene.addComponent(entity, Transform{});
        m_Scene.addComponent(entity, MeshRenderer{ meshID });
        return entity;
    }

    /** @brief A scene document whose MeshRenderer section is @p meshSection, entity 0 only. */
    static nlohmann::json documentWithMeshSection(nlohmann::json meshSection) {
        nlohmann::json root;
        root[meshKey] = std::move(meshSection);
        return root;
    }
};

TEST_F(SceneSerializerTest, RoundTripKeepsEveryComponent) {
    const ECS::Entity player = addMeshEntity(0);
    m_Scene.getPool<Transform>().get(player).position = { 1.0f, 2.0f, 3.0f };
    m_Scene.getPool<Transform>().get(player).scale = { 2.0f, 2.0f, 2.0f };
    m_Scene.addComponent(player, Tag{ "Player" });
    m_Scene.addComponent(player, Material{ "Metal" });
    const ECS::Entity cube = addMeshEntity(7);
    m_Scene.addComponent(cube, Tag{ "Crate \xC3\xBC" });

    ECS::ComponentRegistry loaded;
    ASSERT_TRUE(SceneSerializer::fromJson(SceneSerializer::toJson(m_Scene, m_Meshes), loaded, m_Meshes));

    EXPECT_EQ(poolJson<Transform>(loaded), poolJson<Transform>(m_Scene));
    EXPECT_EQ(poolJson<Tag>(loaded), poolJson<Tag>(m_Scene));
    EXPECT_EQ(poolJson<Material>(loaded), poolJson<Material>(m_Scene));
    EXPECT_EQ(poolJson<MeshRenderer>(loaded), poolJson<MeshRenderer>(m_Scene));
    EXPECT_EQ(sortedEntities(loaded), sortedEntities(m_Scene));
}

TEST_F(SceneSerializerTest, MeshIsSavedAsGuidAndNameNotAsSessionId) {
    addMeshEntity(0);
    addMeshEntity(7);

    const nlohmann::json meshSection = SceneSerializer::toJson(m_Scene, m_Meshes).at(meshKey);

    EXPECT_EQ(meshSection.at("meshGuids"), nlohmann::json::array({ apoGuid.toString(), "" }));
    EXPECT_EQ(meshSection.at("meshNames"), nlohmann::json::array({ "apo", "Default_Cube" }));
    EXPECT_FALSE(meshSection.contains("components")) << "raw mesh IDs must not end up in the file";
}

TEST_F(SceneSerializerTest, GuidWinsOverAnOutdatedName) {
    // The file was saved when the model was still called "CustomModel"; it has been renamed since.
    const nlohmann::json document = documentWithMeshSection({
        { "entities", { 0 } }, { "meshGuids", { apoGuid.toString() } }, { "meshNames", { "CustomModel" } } });

    ECS::ComponentRegistry loaded;
    ASSERT_TRUE(SceneSerializer::fromJson(document, loaded, m_Meshes));

    EXPECT_EQ(loaded.getPool<MeshRenderer>().get(0).meshID, 0u);
}

TEST_F(SceneSerializerTest, MeshWithoutGuidIsFoundByName) {
    // Built-in primitives have no file and therefore no Guid.
    const nlohmann::json document = documentWithMeshSection({
        { "entities", { 0 } }, { "meshGuids", { "" } }, { "meshNames", { "Default_Cube" } } });

    ECS::ComponentRegistry loaded;
    ASSERT_TRUE(SceneSerializer::fromJson(document, loaded, m_Meshes));

    EXPECT_EQ(loaded.getPool<MeshRenderer>().get(0).meshID, 7u);
}

TEST_F(SceneSerializerTest, SceneSavedBeforeGuidsExistedLoadsByName) {
    const nlohmann::json document = documentWithMeshSection({ { "entities", { 0 } }, { "meshNames", { "apo" } } });

    ECS::ComponentRegistry loaded;
    ASSERT_TRUE(SceneSerializer::fromJson(document, loaded, m_Meshes));

    EXPECT_EQ(loaded.getPool<MeshRenderer>().get(0).meshID, 0u);
}

TEST_F(SceneSerializerTest, UnknownGuidFallsBackToTheName) {
    // asset_db.json was lost, so the asset got a new Guid - the name still finds it.
    const nlohmann::json document = documentWithMeshSection({
        { "entities", { 0 } }, { "meshGuids", { "0123456789abcdef0123456789abcdef" } }, { "meshNames", { "apo" } } });

    ECS::ComponentRegistry loaded;
    ASSERT_TRUE(SceneSerializer::fromJson(document, loaded, m_Meshes));

    EXPECT_EQ(loaded.getPool<MeshRenderer>().get(0).meshID, 0u);
}

TEST_F(SceneSerializerTest, MissingMeshDropsOnlyTheMeshRendererAndWarns) {
    const ECS::Entity entity = addMeshEntity(0);
    m_Scene.addComponent(entity, Tag{ "Statue" });
    const nlohmann::json document = SceneSerializer::toJson(m_Scene, m_Meshes);
    FakeMeshLookup withoutTheModel; // the model file was deleted in the meantime

    LogCapture log;
    ECS::ComponentRegistry loaded;
    ASSERT_TRUE(SceneSerializer::fromJson(document, loaded, withoutTheModel));

    EXPECT_FALSE(loaded.hasComponent<MeshRenderer>(entity));
    EXPECT_TRUE(loaded.hasComponent<Transform>(entity));
    EXPECT_EQ(loaded.getPool<Tag>().get(entity).name, "Statue");
    EXPECT_TRUE(log.contains("mesh 'apo' not found")) << log.text();
}

TEST_F(SceneSerializerTest, DamagedDocumentLeavesTheOpenSceneUntouched) {
    const ECS::Entity existing = addMeshEntity(7);
    const nlohmann::json before = SceneSerializer::toJson(m_Scene, m_Meshes);
    // The Transform section is fine, the MeshRenderer section lacks its names: reading fails halfway.
    nlohmann::json damaged = before;
    damaged[meshKey].erase("meshNames");

    LogCapture log;
    EXPECT_FALSE(SceneSerializer::fromJson(damaged, m_Scene, m_Meshes));

    EXPECT_EQ(SceneSerializer::toJson(m_Scene, m_Meshes), before);
    EXPECT_TRUE(m_Scene.hasComponent<MeshRenderer>(existing));
}

TEST_F(SceneSerializerTest, NewEntitiesAfterLoadingDoNotReuseLoadedIds) {
    const nlohmann::json document = documentWithMeshSection({
        { "entities", { 4, 9 } }, { "meshGuids", { "", "" } }, { "meshNames", { "Default_Cube", "apo" } } });

    ECS::ComponentRegistry loaded;
    ASSERT_TRUE(SceneSerializer::fromJson(document, loaded, m_Meshes));

    EXPECT_EQ(loaded.createEntity(), 10u);
}

TEST_F(SceneSerializerTest, SaveAndLoadThroughAFile) {
    TempDirectory dir;
    const std::string file = (dir / "level.scene").string();
    const ECS::Entity entity = addMeshEntity(0);
    m_Scene.addComponent(entity, Tag{ "Player" });

    ASSERT_TRUE(SceneSerializer::save(file, m_Scene, m_Meshes));
    ECS::ComponentRegistry loaded;
    ASSERT_TRUE(SceneSerializer::load(file, loaded, m_Meshes));

    EXPECT_EQ(poolJson<Tag>(loaded), poolJson<Tag>(m_Scene));
    EXPECT_EQ(poolJson<MeshRenderer>(loaded), poolJson<MeshRenderer>(m_Scene));
}

TEST_F(SceneSerializerTest, MissingOrBrokenFileLeavesTheOpenSceneUntouched) {
    TempDirectory dir;
    dir.write("broken.scene", "{ \"Transform\": ");
    addMeshEntity(7);
    const nlohmann::json before = SceneSerializer::toJson(m_Scene, m_Meshes);

    LogCapture log;
    EXPECT_FALSE(SceneSerializer::load((dir / "missing.scene").string(), m_Scene, m_Meshes));
    EXPECT_FALSE(SceneSerializer::load((dir / "broken.scene").string(), m_Scene, m_Meshes));

    EXPECT_EQ(SceneSerializer::toJson(m_Scene, m_Meshes), before);
}
