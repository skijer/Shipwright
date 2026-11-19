#ifndef NEI_PARTNER_CONTROL_V1_H
#define NEI_PARTNER_CONTROL_V1_H
typedef struct {
    u32 size;
    const char* key;
    const char* name;
    Actor* (*actor)(PlayState*);
    bool (*begin)(PlayState*);
    void (*end)(PlayState*);
    void (*input)(PlayState*, const Input*, s16 cameraYaw);
    u32 flags;
} NeiPartnerV1;
#define NEI_PARTNER_TRACK_PLAYER 1u
typedef bool (*NeiRegisterPartnerV1)(const NeiPartnerV1*);
#endif
