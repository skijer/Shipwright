#pragma once
#include "session.h"

namespace pak {
enum BodyType : int32_t { BODY_OOT_CHILD, BODY_OOT_ADULT, BODY_MM_CHILD, BODY_MM_DEITY, BODY_TYPE_COUNT };

static const char* const kBodyTypeLabels[] = { "OoT Child", "OoT Adult", "MM Child", "MM Fierce Deity" };

static BodyType BodyTypeOf(const Model& model) {
    if (model.deity) {
        return BODY_MM_DEITY;
    }
    if (model.mm) {
        return BODY_MM_CHILD;
    }
    return model.child ? BODY_OOT_CHILD : BODY_OOT_ADULT;
}

struct AppearanceRow {
    const char* label;
    const char* keyCVar;
    const char* typeCVar;
    const char* scaleCVar;
    BodyType defaultType;
};

class ModelPicker {
  public:
    void Build(const Library& library) {
        for (const auto& model : library.models) {
            lists[BodyTypeOf(*model)].Add(model->key, Library::Describe(model->label, model->author));
        }
        for (auto& list : lists) {
            list.Finish();
        }
    }

    int32_t CurrentType(const AppearanceRow& row, const Library& library) const {
        if (const Model* model = library.Find(CVarGetString(row.keyCVar, ""))) {
            return BodyTypeOf(*model);
        }
        int32_t type = CVarGetInteger(row.typeCVar, row.defaultType);
        return type >= 0 && type < BODY_TYPE_COUNT ? type : row.defaultType;
    }

    bool Draw(const AppearanceRow& row, const Library& library) {
        ModUI_PushID(row.label);
        ModUI_TextWrapped(row.label);
        float width = 0.f;
        float height = 0.f;
        ModUI_GetContentRegionAvailable(&width, &height);
        bool changed = DrawType(row, library, width * 0.28f);
        ModUI_SameLine(0.f, -1.f);
        changed |= DrawModel(row, library, width * 0.44f);
        ModUI_SameLine(0.f, -1.f);
        DrawScale(row);
        ModUI_PopID();
        return changed;
    }

  private:
    ChoiceList lists[BODY_TYPE_COUNT] = { ChoiceList("Default Link"), ChoiceList("Default Link"),
                                          ChoiceList("Default Link"), ChoiceList("Default Link") };

    bool DrawType(const AppearanceRow& row, const Library& library, float width) {
        int32_t type = CurrentType(row, library);
        bool changed = false;
        ModUI_SetNextItemWidth(width);
        if (ModUI_Combo("##type", &type, kBodyTypeLabels, BODY_TYPE_COUNT)) {
            CVarSetInteger(row.typeCVar, type);
            std::string key = CVarGetString(row.keyCVar, "");
            if (!key.empty() && lists[type].IndexOf(key) == 0) {
                CVarSetString(row.keyCVar, "");
                changed = true;
            }
            CVarSave();
        }
        if (ModUI_IsItemHovered()) {
            ModUI_SetTooltip("Kind of body listed next: Ocarina of Time child or adult, Majora's Mask child or Fierce "
                             "Deity. Any kind fits any appearance.");
        }
        return changed;
    }

    bool DrawModel(const AppearanceRow& row, const Library& library, float width) {
        const ChoiceList& list = lists[CurrentType(row, library)];
        int32_t index = list.IndexOf(CVarGetString(row.keyCVar, ""));
        ModUI_SetNextItemWidth(width);
        if (!ModUI_Combo("##model", &index, list.values.data(), int32_t(list.values.size()))) {
            return false;
        }
        CVarSetString(row.keyCVar, list.keys[index].c_str());
        CVarSave();
        return true;
    }

    void DrawScale(const AppearanceRow& row) {
        float scale = CVarGetFloat(row.scaleCVar, 1.f);
        ModUI_SetNextItemWidth(-1.f);
        if (ModUI_SliderFloat("##scale", &scale, 0.25f, 2.5f)) {
            CVarSetFloat(row.scaleCVar, scale);
        }
        if (ModUI_IsItemDeactivatedAfterEdit()) {
            CVarSave();
        }
        if (ModUI_IsItemHovered()) {
            ModUI_SetTooltip("Root height: raise or lower the model if it floats or sinks. 1 keeps the automatic "
                             "height; abilities and gameplay height do not change.");
        }
    }
};
} // namespace pak
