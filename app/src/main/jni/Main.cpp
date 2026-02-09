#include <list>
#include <vector>
#include <cstring>
#include <pthread.h>
#include <thread>
#include <cstring>
#include <string>
#include <jni.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <dlfcn.h>
#include "Includes/Logger.h"
#include "Includes/obfuscate.h"
#include "Includes/Utils.hpp"
#include "Menu/Menu.hpp"
#include "Menu/Jni.hpp"
#include "Includes/Macros.h"

// ======================================
// School of Chaos v1.891 Mod
// Item Spawner + Level Up
// ======================================

// Global Variables
std::string itemNameToSpawn = "";
std::string levelValue = "";
bool spawnItemPressed = false;
bool setLevelPressed = false;
bool listItemsPressed = false;
bool sendCommandPressed = false;
std::string serverCommand = "";
void* g_AttackComponent = nullptr;
void* g_AllItemsArray = nullptr;
int g_AllItemsCount = 0;

// Inbox Sender - Send items via VNL Entertainment inbox
bool sendInboxPressed = false;
std::string inboxItemName = "";
std::string inboxMessage = "*NEW* Gift - You got [1] ";

// ======================================
// Il2Cpp Type Definitions
// ======================================
typedef void* (*Il2CppStringNew_t)(const char* text);
typedef void* (*GetItemByName_t)(void* instance, void* itemName);
typedef void (*ListAdd_t)(void* list, void* item);
typedef void* (*FindObjectsOfTypeAll_t)(void* type);
typedef void* (*GetType_t)(void* assembly, void* name);
typedef void* (*il2cpp_class_from_name_t)(void* image, const char* namespaze, const char* name);
typedef void* (*il2cpp_class_get_type_t)(void* klass);
typedef void* (*il2cpp_domain_get_t)();
typedef void** (*il2cpp_domain_get_assemblies_t)(void* domain, size_t* size);
typedef void* (*il2cpp_assembly_get_image_t)(void* assembly);
typedef void* (*il2cpp_type_get_object_t)(void* type);

// NetworkCore method to give items via server (like old mod)
// NEW: Takes BadNerdItem object, not string (API changed in v1.891)
// Signature: void NetworkCore.GiveItem(BadNerdItem item, int count, string sender, string message, Callback callback)
typedef void (*NetworkCoreGiveItem_t)(void* networkCore, void* badNerdItem, int count, void* senderName, void* message, void* callback);

// SmartFoxServer2X - Direct server communication
// SmartFox.Send(IRequest request) - RVA: 0x29D25CC
typedef void (*SmartFox_Send_t)(void* smartFox, void* request);

// ExtensionRequest constructor (string cmd, ISFSObject params) - RVA: 0x245BDE0
typedef void (*ExtensionRequest_ctor_t)(void* instance, void* cmd, void* params);

// GenericMessageRequest constructor (no params) - RVA: 0x245C650
// Then set fields manually: type at 0x24, room at 0x28, message at 0x30
typedef void (*GenericMessageRequest_ctor_t)(void* instance);

// SFSObject - For creating ISFSObject parameters (from dump.cs v1.891)
// SFSObject.NewInstance() - RVA: 0x245B214 (DISCOVERED BY CLINE 2026-02-09)
typedef void* (*SFSObject_NewInstance_t)();
// SFSObject.PutUtfString(key, val) - RVA: 0x2478528 (DISCOVERED BY CLINE 2026-02-09)
typedef void (*SFSObject_PutUtfString_t)(void* sfsObj, void* key, void* val);
// SFSObject.PutBool(key, val) - RVA: 0x2478028 (DISCOVERED BY CLINE 2026-02-09)
typedef void (*SFSObject_PutBool_t)(void* sfsObj, void* key, bool val);
// SFSObject.PutInt(key, val) - RVA: 0x24783C8 (DISCOVERED BY CLINE 2026-02-09)
typedef void (*SFSObject_PutInt_t)(void* sfsObj, void* key, int val);
typedef void (*SFSObject_PutShort_t)(void* sfsObj, void* key, short val);

// il2cpp_object_new to allocate new objects
typedef void* (*il2cpp_object_new_t)(void* klass);



Il2CppStringNew_t Il2CppStringNew = nullptr;
GetItemByName_t GetItemByName = nullptr;
FindObjectsOfTypeAll_t FindObjectsOfTypeAll = nullptr;
il2cpp_class_from_name_t il2cpp_class_from_name = nullptr;
il2cpp_class_get_type_t il2cpp_class_get_type = nullptr;
il2cpp_domain_get_t il2cpp_domain_get = nullptr;
il2cpp_domain_get_assemblies_t il2cpp_domain_get_assemblies = nullptr;
il2cpp_assembly_get_image_t il2cpp_assembly_get_image = nullptr;
il2cpp_type_get_object_t il2cpp_type_get_object = nullptr;
NetworkCoreGiveItem_t NetworkCoreGiveItem = nullptr;
SmartFox_Send_t SmartFox_Send = nullptr;
ExtensionRequest_ctor_t ExtensionRequest_ctor = nullptr;
GenericMessageRequest_ctor_t GenericMessageRequest_ctor = nullptr;
il2cpp_object_new_t il2cpp_object_new = nullptr;
// SFSObject functions for creating ISFSObject params
SFSObject_NewInstance_t SFSObject_NewInstance = nullptr;
SFSObject_PutUtfString_t SFSObject_PutUtfString = nullptr;
SFSObject_PutBool_t SFSObject_PutBool = nullptr;
SFSObject_PutShort_t SFSObject_PutShort = nullptr;
void* g_SFSObjectClass = nullptr;

void* g_NetworkCore = nullptr;
void* g_SmartFox = nullptr;
void* g_ExtensionRequestClass = nullptr;
void* g_GenericMessageRequestClass = nullptr;
void* g_CurrentRoom = nullptr;


// God Mode & Combat Hacks
bool godModeEnabled = false;
bool damageMultiplierEnabled = false;
float savedMaxHealth = 100.0f;
int damageMultiplier = 10;


// ======================================
// Feature List for Mod Menu
// ======================================
jobjectArray GetFeatureList(JNIEnv *env, jobject context) {
    jobjectArray ret;

    const char *features[] = {
            OBFUSCATE("Category_🎮 School of Chaos Mod"),
            
            // Item Spawner
            OBFUSCATE("Collapse_📦 Item Spawner"),
            OBFUSCATE("CollapseAdd_InputText_Item Name"),
            OBFUSCATE("CollapseAdd_Button_🎁 Spawn Item"),
            OBFUSCATE("CollapseAdd_Button_📋 List All Items"),
            
            // Level Up
            OBFUSCATE("Collapse_⬆️ Level Up"),
            OBFUSCATE("CollapseAdd_InputText_Level Value"),
            OBFUSCATE("CollapseAdd_Button_🚀 Set Level"),
            
            // Server Console - Direct server communication!
            OBFUSCATE("Collapse_🌐 Server Console"),
            OBFUSCATE("CollapseAdd_InputText_Server Command"),
            OBFUSCATE("CollapseAdd_Button_📡 Send Command"),
            OBFUSCATE("CollapseAdd_Button_🔍 Server Info"),
            
            // Inbox Item Sender (VNL Entertainment style)
            OBFUSCATE("Collapse_📬 Inbox Sender"),
            OBFUSCATE("CollapseAdd_InputText_Item to Send"),
            OBFUSCATE("CollapseAdd_Button_📨 Send to Inbox"),
            
            // Combat Hacks
            OBFUSCATE("Collapse_⚔️ Combat Hacks"),
            OBFUSCATE("CollapseAdd_Toggle_🛡️ God Mode"),
            OBFUSCATE("CollapseAdd_Toggle_💪 Damage x10"),
            OBFUSCATE("CollapseAdd_Button_🔍 Scan Health"),
            
            // Info
            OBFUSCATE("Category_ℹ️ Info"),
            OBFUSCATE("RichTextView_<b>Use /sendItemz [item] to spawn!</b>"),
    };

    int Total_Feature = (sizeof features / sizeof features[0]);
    ret = (jobjectArray)
            env->NewObjectArray(Total_Feature, env->FindClass(OBFUSCATE("java/lang/String")),
                                env->NewStringUTF(""));

    for (int i = 0; i < Total_Feature; i++)
        env->SetObjectArrayElement(ret, i, env->NewStringUTF(features[i]));

    return (ret);
}

// ======================================
// Menu Changes Handler
// ======================================
void Changes(JNIEnv *env, jclass clazz, jobject obj, jint featNum, jstring featName, jint value, jlong Lvalue, jboolean boolean, jstring text) {
    
    const char *featureNameChar = env->GetStringUTFChars(featName, nullptr);
    const char *textChar = text != nullptr ? env->GetStringUTFChars(text, nullptr) : "";
    
    LOGI("Feature: %s, FeatNum: %d, Text: %s", featureNameChar, featNum, textChar);
    
    switch (featNum) {
        case 0: // Item Name Input
            if (text != nullptr && strlen(textChar) > 0) {
                itemNameToSpawn = textChar;
                LOGI("Item name set to: %s", itemNameToSpawn.c_str());
            }
            break;
            
        case 1: // Spawn Item Button
            if (!itemNameToSpawn.empty()) {
                spawnItemPressed = true;
                LOGI("Spawn button pressed for: %s", itemNameToSpawn.c_str());
            }
            break;
            
        case 2: // List All Items Button
            listItemsPressed = true;
            LOGI("List all items button pressed");
            break;
            
        case 3: // Level Value Input
            if (text != nullptr && strlen(textChar) > 0) {
                levelValue = textChar;
                LOGI("Level value set to: %s", levelValue.c_str());
            }
            break;
            
        case 4: // Set Level Button
            if (!levelValue.empty()) {
                setLevelPressed = true;
                LOGI("Set level pressed: %s", levelValue.c_str());
            }
            break;
            
        case 5: // Server Command Input
            if (text != nullptr && strlen(textChar) > 0) {
                serverCommand = textChar;
                LOGI("Server command set to: %s", serverCommand.c_str());
            }
            break;
            
        case 6: // Send Command Button
            if (!serverCommand.empty()) {
                sendCommandPressed = true;
                LOGI("Send command pressed: %s", serverCommand.c_str());
            }
            break;
            
        case 7: // Server Info Button - Safe scan for connection details
            LOGI("=== SCANNING SERVER CONNECTION INFO ===");
            if (g_SmartFox != nullptr) {
                LOGI("SmartFox instance: %p", g_SmartFox);
                
                // Only scan for port numbers (safe - just reading ints from the object)
                LOGI("--- Scanning for port numbers ---");
                for (int offset = 0; offset < 0x100; offset += 4) {
                    int val = *(int*)((uintptr_t)g_SmartFox + offset);
                    if (val >= 1000 && val <= 65535) {
                        LOGI("[0x%X] Possible Port: %d", offset, val);
                    }
                }
                
                // Log NetworkCore instance too
                if (g_NetworkCore != nullptr) {
                    LOGI("NetworkCore instance: %p", g_NetworkCore);
                }
                
                LOGI("TIP: Use Wireshark/tcpdump to capture actual traffic!");
            } else {
                LOGI("ERROR: SmartFox not initialized! Send a command first.");
            }
            LOGI("=== END SERVER SCAN ===");
            break;
            
        case 8: // Item to Send Input (Inbox)
            if (text != nullptr && strlen(textChar) > 0) {
                inboxItemName = textChar;
                inboxMessage = "*NEW* Gift - You got [1] " + inboxItemName + "!";
                LOGI("Inbox item set to: %s", inboxItemName.c_str());
            }
            break;
            
        case 9: // Send to Inbox Button
            if (!inboxItemName.empty()) {
                sendInboxPressed = true;
                LOGI("Send to Inbox pressed for: %s", inboxItemName.c_str());
            }
            break;
            
        case 10: // God Mode Toggle
            godModeEnabled = boolean;
            LOGI("God Mode: %s", godModeEnabled ? "ENABLED" : "DISABLED");
            break;
            
        case 11: // Damage x10 Toggle
            damageMultiplierEnabled = boolean;
            LOGI("Damage x10: %s", damageMultiplierEnabled ? "ENABLED" : "DISABLED");
            break;
            
        case 12: // Scan Health Button
            LOGI("=== SCANNING FOR HEALTH VALUES ===");
            if (g_AttackComponent != nullptr) {
                LOGI("AttackComponent: %p", g_AttackComponent);
                // Scan first 0x200 bytes for float values that look like health
                for (int offset = 0; offset < 0x200; offset += 4) {
                    float val = *(float*)((uintptr_t)g_AttackComponent + offset);
                    // Health is usually between 1 and 1000
                    if (val > 0.0f && val < 10000.0f && val == (int)val) {
                        LOGI("[0x%X] = %.1f", offset, val);
                    }
                }
            } else {
                LOGI("ERROR: AttackComponent not captured yet!");
            }
            LOGI("=== END SCAN ===");
            break;
    }
    
    if (text != nullptr) {
        env->ReleaseStringUTFChars(text, textChar);
    }
    env->ReleaseStringUTFChars(featName, featureNameChar);
}

// ======================================
// Hook Functions
// ======================================

// Function pointer for SFSObject.Dump() - RVA: 0x2476F10
typedef void* (*SFSObject_Dump_t)(void* sfsObject);
SFSObject_Dump_t SFSObject_Dump = nullptr;

// Hook SmartFox.Send to intercept ALL server requests
void (*old_SmartFox_Send)(void* smartFox, void* request);
void SmartFox_Send_Hook(void* smartFox, void* request) {
    
    if (request != nullptr) {
        // BaseRequest.id is at offset 0x18 (int)
        int requestId = *(int*)((uintptr_t)request + 0x18);
        
        // Only log interesting requests (not the constant "c" spam)
        // ExtensionRequest: extCmd at 0x28, parameters at 0x30
        if (requestId == 13) { // CALL_EXTENSION
            void* extCmdStr = *(void**)((uintptr_t)request + 0x28);
            if (extCmdStr != nullptr) {
                int strLen = *(int*)((uintptr_t)extCmdStr + 0x10);
                if (strLen > 0 && strLen < 256) {
                    char16_t* chars = (char16_t*)((uintptr_t)extCmdStr + 0x14);
                    char cmdBuf[257] = {0};
                    for (int i = 0; i < strLen && i < 256; i++) {
                        cmdBuf[i] = (char)chars[i];
                    }
                    
                    // Skip the constant "c" and "z" spam, only log interesting commands
                    if (cmdBuf[0] != 'c' && cmdBuf[0] != 'z') {
                        LOGI("=== EXTENSION REQUEST: cmd='%s' ===", cmdBuf);
                        
                        // Try to dump the ISFSObject parameters
                        void* params = *(void**)((uintptr_t)request + 0x30);
                        if (params != nullptr && SFSObject_Dump != nullptr) {
                            void* dumpStr = SFSObject_Dump(params);
                            if (dumpStr != nullptr) {
                                int dumpLen = *(int*)((uintptr_t)dumpStr + 0x10);
                                if (dumpLen > 0 && dumpLen < 2048) {
                                    char16_t* dumpChars = (char16_t*)((uintptr_t)dumpStr + 0x14);
                                    char dumpBuf[2049] = {0};
                                    for (int i = 0; i < dumpLen && i < 2048; i++) {
                                        dumpBuf[i] = (char)dumpChars[i];
                                    }
                                    LOGI("Params: %s", dumpBuf);
                                }
                            }
                        }
                    }
                }
            }
        } else if (requestId == 7) { // GENERIC_MESSAGE - chat messages!
            // GenericMessageRequest: type at 0x24, room at 0x28, message at 0x30
            void* messageStr = *(void**)((uintptr_t)request + 0x30);
            if (messageStr != nullptr) {
                int strLen = *(int*)((uintptr_t)messageStr + 0x10);
                if (strLen > 0 && strLen < 512) {
                    char16_t* chars = (char16_t*)((uintptr_t)messageStr + 0x14);
                    char msgBuf[513] = {0};
                    for (int i = 0; i < strLen && i < 512; i++) {
                        msgBuf[i] = (char)chars[i];
                    }
                    
                    // Skip /vnlPing spam - only log user commands
                    if (strncmp(msgBuf, "/vnlPing", 8) != 0) {
                        LOGI("=== CHAT MESSAGE: '%s' ===", msgBuf);
                        
                        // Check if it's a command (starts with /)
                        if (msgBuf[0] == '/') {
                            LOGI(">>> COMMAND DETECTED! <<<");
                        }
                    }
                }
            }
        } else if (requestId != 12) { // Skip JoinRoom spam (ID 12)
            LOGI("=== Request ID: %d ===", requestId);
        }
    }
    
    // Call original
    old_SmartFox_Send(smartFox, request);
}

// Hook Update to capture AttackComponent instance
void (*old_Update)(void *instance);
void Update(void *instance) {
    if (instance != nullptr) {
        // Save the instance
        if (g_AttackComponent == nullptr) {
            g_AttackComponent = instance;
            LOGI("PlayerAttackComponent captured: %p", instance);
        }
        
        // ============ GOD MODE ============
        // Try common health offsets - these are typical Unity/IL2CPP patterns
        // We'll try multiple offsets and set them all to high values
        if (godModeEnabled && g_AttackComponent == instance) {
            // Common health field offsets to try (different games use different offsets)
            static const int healthOffsets[] = {
                0x48, 0x4C, 0x50, 0x54, 0x58, 0x5C, 0x60, 0x64, 0x68, 0x6C,
                0x70, 0x74, 0x78, 0x7C, 0x80, 0x84, 0x88, 0x8C, 0x90, 0x94,
                0xA0, 0xA4, 0xA8, 0xAC, 0xB0, 0xB4, 0xB8, 0xBC, 0xC0, 0xC4
            };
            
            for (int i = 0; i < sizeof(healthOffsets)/sizeof(healthOffsets[0]); i++) {
                int offset = healthOffsets[i];
                float* healthPtr = (float*)((uintptr_t)instance + offset);
                float currentVal = *healthPtr;
                
                // If value looks like health (positive, not too high, getting lower means damage)
                // Set it to max
                if (currentVal > 0.0f && currentVal < savedMaxHealth) {
                    // Someone took damage, restore to max
                    *healthPtr = savedMaxHealth;
                } else if (currentVal > savedMaxHealth && currentVal < 10000.0f) {
                    // Found a higher max health, save it
                    savedMaxHealth = currentVal;
                }
            }
        }
        
        // Handle Item Spawning - TRUE SPAWNER
        if (spawnItemPressed) {
            spawnItemPressed = false;
            LOGI("=== TRUE ITEM SPAWN ===");
            LOGI("Target item: %s", itemNameToSpawn.c_str());
            
            // Use GetItemByName directly - much simpler than manual search!
            void* foundItem = nullptr;
            
            if (GetItemByName != nullptr && Il2CppStringNew != nullptr) {
                void* itemNameStr = Il2CppStringNew(itemNameToSpawn.c_str());
                LOGI("Created item name string: %p", itemNameStr);
                
                // Call GetItemByName(instance, itemName) 
                foundItem = GetItemByName(instance, itemNameStr);
                
                if (foundItem != nullptr) {
                    LOGI("FOUND item via GetItemByName: %p", foundItem);
                } else {
                    LOGI("GetItemByName returned null - item '%s' not found", itemNameToSpawn.c_str());
                }
            } else {
                LOGI("ERROR: GetItemByName not initialized!");
            }
            
            // Give item via NetworkCore (server-side, like old mod)
            // Items will appear in Inbox from "VNL Entertainment"
            if (NetworkCoreGiveItem != nullptr && Il2CppStringNew != nullptr) {
                // Get NetworkCore instance if not already cached
                if (g_NetworkCore == nullptr && il2cpp_domain_get != nullptr) {
                    LOGI("Looking for NetworkCore instance...");
                    
                    void* domain = il2cpp_domain_get();
                    if (domain != nullptr) {
                        size_t assemblyCount = 0;
                        void** assemblies = il2cpp_domain_get_assemblies(domain, &assemblyCount);
                        
                        for (size_t i = 0; i < assemblyCount; i++) {
                            void* image = il2cpp_assembly_get_image(assemblies[i]);
                            if (image != nullptr) {
                                void* networkCoreClass = il2cpp_class_from_name(image, "", "NetworkCore");
                                if (networkCoreClass != nullptr) {
                                    LOGI("Found NetworkCore class: %p", networkCoreClass);
                                    
                                    void* type = il2cpp_class_get_type(networkCoreClass);
                                    if (type != nullptr) {
                                        void* typeObject = il2cpp_type_get_object(type);
                                        if (FindObjectsOfTypeAll != nullptr && typeObject != nullptr) {
                                            void* networkCoreArray = FindObjectsOfTypeAll(typeObject);
                                            if (networkCoreArray != nullptr) {
                                                int count = *(int*)((uintptr_t)networkCoreArray + 0x18);
                                                LOGI("Found %d NetworkCore instances", count);
                                                if (count > 0) {
                                                    void** items = (void**)((uintptr_t)networkCoreArray + 0x20);
                                                    g_NetworkCore = items[0];
                                                    LOGI("NetworkCore instance: %p", g_NetworkCore);
                                                }
                                            }
                                        }
                                    }
                                    break;
                                }
                            }
                        }
                    }
                }
                
                if (g_NetworkCore != nullptr) {
                    // Check if we found the item
                    if (foundItem != nullptr) {
                        LOGI("=== SENDING TO SERVER ===");
                        LOGI("NetworkCore instance: %p", g_NetworkCore);
                        LOGI("Item to send: %p (%s)", foundItem, itemNameToSpawn.c_str());
                        
                        // Use "VNL Entertainment" as sender (like old mod command)
                        void* senderStr = Il2CppStringNew("VNL Entertainment");
                        void* messageStr = Il2CppStringNew("");
                        
                        LOGI("Calling NetworkCore.GiveItem with BadNerdItem...");
                        LOGI("Params: networkCore=%p, item=%p, count=1, sender=%p, msg=%p, callback=null", 
                             g_NetworkCore, foundItem, senderStr, messageStr);
                        
                        // Call server method: GiveItem(BadNerdItem, count=1, senderName, message, callback)
                        NetworkCoreGiveItem(g_NetworkCore, foundItem, 1, senderStr, messageStr, nullptr);
                        
                        LOGI("SUCCESS! Item request sent to server!");
                        LOGI("Check your Inbox for: %s", itemNameToSpawn.c_str());
                    } else {
                        LOGI("ERROR: Item '%s' not found in loaded items!", itemNameToSpawn.c_str());
                        LOGI("Try: Energy Drink, Apple, Steel Bat, etc.");
                    }
                } else {
                    LOGI("ERROR: NetworkCore instance not found!");
                    LOGI("Make sure you are connected to the game server!");
                }
            } else {
                LOGI("ERROR: NetworkCoreGiveItem not initialized!");
            }
            LOGI("=== END SPAWN ===");
        }
        
        // Handle Level Setting
        if (setLevelPressed && Il2CppStringNew != nullptr) {
            setLevelPressed = false;
            LOGI("=== SETTING LEVEL ===");
            
            void* levelStr = Il2CppStringNew(levelValue.c_str());
            LOGI("Created level string: %p", levelStr);
            
            if (levelStr != nullptr) {
                *(void**)((uintptr_t)instance + 0x198) = levelStr;
                LOGI("Level set to: %s", levelValue.c_str());
            }
            LOGI("=== END LEVEL ===");
        }
        
        // Handle List All Items - Save to file
        if (listItemsPressed && il2cpp_domain_get != nullptr) {
            listItemsPressed = false;
            LOGI("=== LISTING ALL ITEMS ===");
            
            // Open file for writing
            FILE* itemsFile = fopen("/sdcard/items.txt", "w");
            if (itemsFile == nullptr) {
                LOGI("ERROR: Cannot create /sdcard/items.txt!");
                LOGI("Try: /data/local/tmp/items.txt instead");
                itemsFile = fopen("/data/local/tmp/items.txt", "w");
            }
            
            void* domain = il2cpp_domain_get();
            if (domain != nullptr) {
                size_t assemblyCount = 0;
                void** assemblies = il2cpp_domain_get_assemblies(domain, &assemblyCount);
                
                for (size_t i = 0; i < assemblyCount; i++) {
                    void* image = il2cpp_assembly_get_image(assemblies[i]);
                    if (image != nullptr) {
                        void* badNerdItemClass = il2cpp_class_from_name(image, "", "BadNerdItem");
                        if (badNerdItemClass != nullptr) {
                            void* type = il2cpp_class_get_type(badNerdItemClass);
                            if (type != nullptr) {
                                void* typeObject = il2cpp_type_get_object(type);
                                if (FindObjectsOfTypeAll != nullptr && typeObject != nullptr) {
                                    void* itemsArray = FindObjectsOfTypeAll(typeObject);
                                    if (itemsArray != nullptr) {
                                        int count = *(int*)((uintptr_t)itemsArray + 0x18);
                                        void** items = (void**)((uintptr_t)itemsArray + 0x20);
                                        
                                        LOGI("=== TOTAL ITEMS LOADED: %d ===", count);
                                        
                                        if (itemsFile) {
                                            fprintf(itemsFile, "=== School of Chaos Items (%d total) ===\n\n", count);
                                        }
                                        
                                        for (int idx = 0; idx < count; idx++) {
                                            void* item = items[idx];
                                            if (item != nullptr) {
                                                // Get item name from offset 0x30
                                                void* namePtr = *(void**)((uintptr_t)item + 0x30);
                                                if (namePtr != nullptr) {
                                                    // Il2CppString: length at 0x10, chars at 0x14
                                                    int nameLen = *(int*)((uintptr_t)namePtr + 0x10);
                                                    if (nameLen > 0 && nameLen < 200) {
                                                        char16_t* nameChars = (char16_t*)((uintptr_t)namePtr + 0x14);
                                                        // Convert to ASCII
                                                        char nameBuffer[256] = {0};
                                                        for (int c = 0; c < nameLen && c < 255; c++) {
                                                            nameBuffer[c] = (char)(nameChars[c] & 0xFF);
                                                        }
                                                        
                                                        if (itemsFile) {
                                                            fprintf(itemsFile, "%d. %s\n", idx + 1, nameBuffer);
                                                        }
                                                        
                                                        // Log first 20 items to logcat too
                                                        if (idx < 20) {
                                                            LOGI("[%d] %s", idx, nameBuffer);
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                        
                                        if (itemsFile) {
                                            LOGI("=== Items saved to /sdcard/items.txt ===");
                                        }
                                    }
                                }
                            }
                            break;
                        }
                    }
                }
            }
            
            if (itemsFile) {
                fclose(itemsFile);
            }
            LOGI("=== END ITEM LIST ===");
        }
    }
    
    // Handle Send Command - Direct server communication via SmartFox
    if (sendCommandPressed) {
        sendCommandPressed = false;
        LOGI("=== SENDING SERVER COMMAND ===");
        LOGI("Command: %s", serverCommand.c_str());
        
        // First, we need to find NetworkCore and get SmartFox from it
        if (g_SmartFox == nullptr && il2cpp_domain_get != nullptr) {
            // Try to find NetworkCore instance
            void* domain = il2cpp_domain_get();
            if (domain != nullptr) {
                size_t assembliesCount = 0;
                void** assemblies = il2cpp_domain_get_assemblies(domain, &assembliesCount);
                
                for (size_t i = 0; i < assembliesCount; i++) {
                    void* image = il2cpp_assembly_get_image(assemblies[i]);
                    if (image == nullptr) continue;
                    
                    void* networkCoreClass = il2cpp_class_from_name(image, "", "NetworkCore");
                    if (networkCoreClass != nullptr) {
                        LOGI("Found NetworkCore class: %p", networkCoreClass);
                        
                        // Get type and find instances
                        void* type = il2cpp_class_get_type(networkCoreClass);
                        if (type != nullptr && FindObjectsOfTypeAll != nullptr) {
                            void* typeObject = il2cpp_type_get_object(type);
                            if (typeObject != nullptr) {
                                void* instancesArray = FindObjectsOfTypeAll(typeObject);
                                if (instancesArray != nullptr) {
                                    int count = *(int*)((uintptr_t)instancesArray + 0x18);
                                    if (count > 0) {
                                        void** instances = (void**)((uintptr_t)instancesArray + 0x20);
                                        g_NetworkCore = instances[0];
                                        LOGI("NetworkCore instance: %p", g_NetworkCore);
                                        
                                        // SmartFox is at offset 0xA8 in NetworkCore
                                        g_SmartFox = *(void**)((uintptr_t)g_NetworkCore + 0xA8);
                                        LOGI("SmartFox instance: %p", g_SmartFox);
                                    }
                                }
                            }
                        }
                        break;
                    }
                }
            }
        }
        
        // Now try to send the command
        if (g_SmartFox != nullptr && SmartFox_Send != nullptr && Il2CppStringNew != nullptr) {
            LOGI("Attempting to send command via SmartFox...");
            
            // Create command string
            void* cmdStr = Il2CppStringNew(serverCommand.c_str());
            LOGI("Command string: %p", cmdStr);
            
            // Find GenericMessageRequest class (for chat-style commands like /sendItemz)
            if (g_GenericMessageRequestClass == nullptr) {
                void* domain = il2cpp_domain_get();
                if (domain != nullptr) {
                    size_t assembliesCount = 0;
                    void** assemblies = il2cpp_domain_get_assemblies(domain, &assembliesCount);
                    
                    for (size_t i = 0; i < assembliesCount; i++) {
                        void* image = il2cpp_assembly_get_image(assemblies[i]);
                        if (image == nullptr) continue;
                        
                        // GenericMessageRequest is in Sfs2X.Requests namespace
                        void* genMsgClass = il2cpp_class_from_name(image, "Sfs2X.Requests", "GenericMessageRequest");
                        if (genMsgClass != nullptr) {
                            g_GenericMessageRequestClass = genMsgClass;
                            LOGI("Found GenericMessageRequest class: %p", g_GenericMessageRequestClass);
                            break;
                        }
                    }
                }
            }
            
            // Get current room from SmartFox (needed for public messages)
            // SmartFox.LastJoinedRoom is at offset 0x78
            void* lastJoinedRoom = *(void**)((uintptr_t)g_SmartFox + 0x78);
            LOGI("LastJoinedRoom: %p", lastJoinedRoom);
            
            if (g_GenericMessageRequestClass != nullptr && il2cpp_object_new != nullptr && GenericMessageRequest_ctor != nullptr) {
                // Allocate new GenericMessageRequest object
                void* genMsgRequest = il2cpp_object_new(g_GenericMessageRequestClass);
                LOGI("Allocated GenericMessageRequest: %p", genMsgRequest);
                
                if (genMsgRequest != nullptr) {
                    // Call default constructor
                    GenericMessageRequest_ctor(genMsgRequest);
                    
                    // Set fields manually:
                    // type (int) at 0x24 - PUBLIC_MESSAGE = 0
                    *(int*)((uintptr_t)genMsgRequest + 0x24) = 0; // PUBLIC_MESSAGE type
                    
                    // room (Room) at 0x28 - the current room
                    *(void**)((uintptr_t)genMsgRequest + 0x28) = lastJoinedRoom;
                    
                    // message (string) at 0x30 - the command
                    *(void**)((uintptr_t)genMsgRequest + 0x30) = cmdStr;
                    
                    LOGI("GenericMessageRequest configured with message: %s", serverCommand.c_str());
                    
                    // Send via SmartFox  
                    LOGI("Calling SmartFox.Send with GenericMessageRequest...");
                    SmartFox_Send(g_SmartFox, genMsgRequest);
                    LOGI("=== CHAT COMMAND SENT TO SERVER! ===");
                }
            } else {
                if (g_GenericMessageRequestClass == nullptr) LOGE("GenericMessageRequest class not found!");
                if (il2cpp_object_new == nullptr) LOGE("il2cpp_object_new not available!");
                if (GenericMessageRequest_ctor == nullptr) LOGE("GenericMessageRequest_ctor not initialized!");
            }
        } else {
            if (g_SmartFox == nullptr) LOGE("SmartFox not found!");
            if (SmartFox_Send == nullptr) LOGE("SmartFox_Send not initialized!");
        }
        
        LOGI("=== END SEND COMMAND ===");
    }
    
    // Handle Send to Inbox - Uses ExtensionRequest with sendOfflineMessage command
    if (sendInboxPressed) {
        sendInboxPressed = false;
        LOGI("=== SENDING INBOX MESSAGE ===");
        LOGI("Recipient: self (will send to your own inbox)");
        LOGI("Item/Subject: %s", inboxItemName.c_str());
        
        // Ensure we have SmartFox and NetworkCore instances
        if (g_SmartFox == nullptr && il2cpp_domain_get != nullptr) {
            void* domain = il2cpp_domain_get();
            if (domain != nullptr) {
                size_t assembliesCount = 0;
                void** assemblies = il2cpp_domain_get_assemblies(domain, &assembliesCount);
                
                for (size_t i = 0; i < assembliesCount; i++) {
                    void* image = il2cpp_assembly_get_image(assemblies[i]);
                    if (image == nullptr) continue;
                    
                    void* networkCoreClass = il2cpp_class_from_name(image, "", "NetworkCore");
                    if (networkCoreClass != nullptr) {
                        void* type = il2cpp_class_get_type(networkCoreClass);
                        if (type != nullptr && FindObjectsOfTypeAll != nullptr) {
                            void* typeObject = il2cpp_type_get_object(type);
                            if (typeObject != nullptr) {
                                void* instancesArray = FindObjectsOfTypeAll(typeObject);
                                if (instancesArray != nullptr) {
                                    int count = *(int*)((uintptr_t)instancesArray + 0x18);
                                    if (count > 0) {
                                        void** instances = (void**)((uintptr_t)instancesArray + 0x20);
                                        g_NetworkCore = instances[0];
                                        g_SmartFox = *(void**)((uintptr_t)g_NetworkCore + 0xA8);
                                        LOGI("SmartFox found: %p", g_SmartFox);
                                    }
                                }
                            }
                        }
                        break;
                    }
                }
            }
        }
        
        if (g_SmartFox != nullptr && SmartFox_Send != nullptr && Il2CppStringNew != nullptr) {
            // Find ExtensionRequest class
            if (g_ExtensionRequestClass == nullptr) {
                void* domain = il2cpp_domain_get();
                if (domain != nullptr) {
                    size_t assembliesCount = 0;
                    void** assemblies = il2cpp_domain_get_assemblies(domain, &assembliesCount);
                    
                    for (size_t i = 0; i < assembliesCount; i++) {
                        void* image = il2cpp_assembly_get_image(assemblies[i]);
                        if (image == nullptr) continue;
                        
                        void* extReqClass = il2cpp_class_from_name(image, "Sfs2X.Requests", "ExtensionRequest");
                        if (extReqClass != nullptr) {
                            g_ExtensionRequestClass = extReqClass;
                            LOGI("Found ExtensionRequest class: %p", g_ExtensionRequestClass);
                            break;
                        }
                    }
                }
            }
            
            // Get current room ID from SmartFox.LastJoinedRoom
            void* lastJoinedRoom = *(void**)((uintptr_t)g_SmartFox + 0x78);
            int roomId = 0;
            if (lastJoinedRoom != nullptr) {
                // Room.Id is at offset 0x24
                roomId = *(int*)((uintptr_t)lastJoinedRoom + 0x24);
                LOGI("Current Room ID: %d", roomId);
            }
            
            // Use fixed username from player account
            // TODO: Find correct offset for MySelf.Name extraction
            std::string myUsername = "kjhgasdj16s11";
            LOGI("Using fixed username: %s", myUsername.c_str());
            
            // Build message body with item info
            std::string subject = "*NEW* Gift - " + inboxItemName;
            std::string msgBody = "You received: " + inboxItemName + "\n\nFrom: VNL Entertainment";
            
            LOGI("Preparing sendOfflineMessage...");
            LOGI("  toId: %s", myUsername.c_str());
            LOGI("  subject: %s", subject.c_str());
            LOGI("  msgBody: %s", msgBody.c_str());
            
            // Create strings
            void* commandStr = Il2CppStringNew("sendOfflineMessage");
            void* toIdStr = Il2CppStringNew(myUsername.c_str());
            void* subjectStr = Il2CppStringNew(subject.c_str());
            void* msgBodyStr = Il2CppStringNew(msgBody.c_str());
            
            // Create SFSObject params - COMMENTED OUT FOR TESTING
            // Send with nullptr params first to test if crash is in SFSObject functions
            void* sfsParams = nullptr;
            
            LOGI("Sending ExtensionRequest with NULL params (testing for crash)...");
            
            // If we want to add params later, uncomment this block:
            /*
            if (SFSObject_NewInstance != nullptr) {
                sfsParams = SFSObject_NewInstance();
                if (sfsParams != nullptr) {
                    LOGI("Created SFSObject params: %p", sfsParams);
                    // Add params here...
                }
            }
            */
            
            if (g_ExtensionRequestClass != nullptr && il2cpp_object_new != nullptr && ExtensionRequest_ctor != nullptr) {
                // Allocate ExtensionRequest
                void* extRequest = il2cpp_object_new(g_ExtensionRequestClass);
                if (extRequest != nullptr) {
                    LOGI("Allocated ExtensionRequest: %p", extRequest);
                    
                    // Call constructor: ExtensionRequest(string cmd, ISFSObject params)
                    ExtensionRequest_ctor(extRequest, commandStr, sfsParams);
                    
                    LOGI("ExtensionRequest initialized with command: sendOfflineMessage");
                    LOGI("Params: %s", sfsParams != nullptr ? "SFSObject" : "NULL");
                    LOGI("Sending via SmartFox.Send...");
                    
                    SmartFox_Send(g_SmartFox, extRequest);
                    
                    LOGI("=== ExtensionRequest SENT! ===");
                    LOGI("Check server response in logcat");
                }
            } else {
                LOGE("ExtensionRequest class not found!");
                LOGE("ExtensionRequest_ctor: %p", ExtensionRequest_ctor);
                LOGE("g_ExtensionRequestClass: %p", g_ExtensionRequestClass);
            }
        } else {
            LOGE("SmartFox not initialized! Enter a room first.");
        }
        
        LOGI("=== END INBOX SEND ===");
    }

    

    return old_Update(instance);
}

// ======================================
// Target Library
// ======================================
#define targetLibName OBFUSCATE("libil2cpp.so")

ElfScanner g_il2cppELF;

// ======================================
// RVA Offsets for School of Chaos v1.891 (arm64)
// ======================================
#define RVA_GET_ITEM_BY_NAME     0x41C8D24  // AttackComponent.GetItemByName(string)
#define RVA_ATTACK_COMPONENT_UPDATE  0x41CB458  // AttackComponent.Update()
#define RVA_NETWORKCORE_GIVE_ITEM    0x302EC74  // NetworkCore.GiveItem(BadNerdItem, int, string, string, callback)
#define RVA_SMARTFOX_SEND            0x29D25CC  // SmartFox.Send(IRequest request)
#define RVA_EXTENSIONREQUEST_CTOR    0x245BDE0  // ExtensionRequest.ctor(string cmd, ISFSObject params)
#define RVA_GENERICMESSAGE_CTOR      0x245C650  // GenericMessageRequest.ctor()
// SFSObject RVAs - DISCOVERED BY CLINE 2026-02-09 (from dump.cs v1.891)
#define RVA_SFSOBJECT_NEWINSTANCE    0x245B214   // SFSObject.NewInstance() - ✅ CORRECT
#define RVA_SFSOBJECT_PUTUTFSTRING   0x2478528   // SFSObject.PutUtfString(key, val) - ✅ CORRECT
#define RVA_SFSOBJECT_PUTBOOL        0x2478028   // SFSObject.PutBool(key, val) - ✅ CORRECT
#define RVA_SFSOBJECT_PUTINT         0x24783C8   // SFSObject.PutInt(key, val) - ✅ CORRECT
#define RVA_SFSOBJECT_PUTSHORT       0x60A0AC   // SFSObject.PutShort(key, val) - from old dump (unverified)

// ======================================
// Hack Thread
// ======================================
void *hack_thread(void *) {
    LOGI(OBFUSCATE("School of Chaos Mod - Starting..."));

    // Wait for libil2cpp.so to load
    do {
        sleep(1);
        g_il2cppELF = ElfScanner::createWithPath(targetLibName);
    } while (!g_il2cppELF.isValid());

    LOGI(OBFUSCATE("%s has been loaded at base 0x%llX"), (const char *) targetLibName, (long long)g_il2cppELF.base());

#if defined(__aarch64__)
    // Get il2cpp API functions using dlsym
    void* il2cppHandle = dlopen("libil2cpp.so", RTLD_NOLOAD);
    if (il2cppHandle != nullptr) {
        Il2CppStringNew = (Il2CppStringNew_t)dlsym(il2cppHandle, "il2cpp_string_new");
        il2cpp_class_from_name = (il2cpp_class_from_name_t)dlsym(il2cppHandle, "il2cpp_class_from_name");
        il2cpp_class_get_type = (il2cpp_class_get_type_t)dlsym(il2cppHandle, "il2cpp_class_get_type");
        il2cpp_domain_get = (il2cpp_domain_get_t)dlsym(il2cppHandle, "il2cpp_domain_get");
        il2cpp_domain_get_assemblies = (il2cpp_domain_get_assemblies_t)dlsym(il2cppHandle, "il2cpp_domain_get_assemblies");
        il2cpp_assembly_get_image = (il2cpp_assembly_get_image_t)dlsym(il2cppHandle, "il2cpp_assembly_get_image");
        il2cpp_type_get_object = (il2cpp_type_get_object_t)dlsym(il2cppHandle, "il2cpp_type_get_object");
        
        // Get FindObjectsOfTypeAll from RVA
        FindObjectsOfTypeAll = (FindObjectsOfTypeAll_t)getAbsoluteAddress(targetLibName, 0x423CCE4);
        
        LOGI("il2cpp_string_new: %p", Il2CppStringNew);
        LOGI("il2cpp_domain_get: %p", il2cpp_domain_get);
        LOGI("FindObjectsOfTypeAll: %p", FindObjectsOfTypeAll);
    } else {
        LOGE("Failed to get libil2cpp.so handle");
    }
    
    // Get GetItemByName function
    GetItemByName = (GetItemByName_t)getAbsoluteAddress(targetLibName, RVA_GET_ITEM_BY_NAME);
    LOGI("GetItemByName at: %p", GetItemByName);
    
    // Get NetworkCore.GiveItemByName function (for server-side item spawning)
    NetworkCoreGiveItem = (NetworkCoreGiveItem_t)getAbsoluteAddress(targetLibName, RVA_NETWORKCORE_GIVE_ITEM);
    LOGI("NetworkCoreGiveItem at: %p", NetworkCoreGiveItem);
    
    // Get SmartFox.Send function (for direct server communication)
    SmartFox_Send = (SmartFox_Send_t)getAbsoluteAddress(targetLibName, RVA_SMARTFOX_SEND);
    LOGI("SmartFox_Send at: %p", SmartFox_Send);
    
    // Get ExtensionRequest constructor (for creating server commands)
    ExtensionRequest_ctor = (ExtensionRequest_ctor_t)getAbsoluteAddress(targetLibName, RVA_EXTENSIONREQUEST_CTOR);
    LOGI("ExtensionRequest_ctor at: %p", ExtensionRequest_ctor);
    
    // Get GenericMessageRequest constructor (for chat-style commands like /sendItemz)
    GenericMessageRequest_ctor = (GenericMessageRequest_ctor_t)getAbsoluteAddress(targetLibName, RVA_GENERICMESSAGE_CTOR);
    LOGI("GenericMessageRequest_ctor at: %p", GenericMessageRequest_ctor);
    
    // Initialize SFSObject functions for creating params
    SFSObject_NewInstance = (SFSObject_NewInstance_t)getAbsoluteAddress(targetLibName, RVA_SFSOBJECT_NEWINSTANCE);
    SFSObject_PutUtfString = (SFSObject_PutUtfString_t)getAbsoluteAddress(targetLibName, RVA_SFSOBJECT_PUTUTFSTRING);
    SFSObject_PutBool = (SFSObject_PutBool_t)getAbsoluteAddress(targetLibName, RVA_SFSOBJECT_PUTBOOL);
    SFSObject_PutShort = (SFSObject_PutShort_t)getAbsoluteAddress(targetLibName, RVA_SFSOBJECT_PUTSHORT);
    LOGI("SFSObject_NewInstance at: %p", SFSObject_NewInstance);
    LOGI("SFSObject_PutUtfString at: %p", SFSObject_PutUtfString);
    LOGI("SFSObject_PutBool at: %p", SFSObject_PutBool);
    
    // Get il2cpp_object_new via dlsym for allocating new objects
    if (il2cppHandle != nullptr) {
        il2cpp_object_new = (il2cpp_object_new_t)dlsym(il2cppHandle, "il2cpp_object_new");
        LOGI("il2cpp_object_new at: %p", il2cpp_object_new);
    }
    
    // Get SFSObject.Dump for debugging request parameters
    SFSObject_Dump = (SFSObject_Dump_t)getAbsoluteAddress(targetLibName, 0x2476F10);
    LOGI("SFSObject_Dump at: %p", SFSObject_Dump);
    
    // Hook SmartFox.Send to intercept ALL requests!
    HOOK(targetLibName, RVA_SMARTFOX_SEND, SmartFox_Send_Hook, old_SmartFox_Send);
    LOGI("SmartFox.Send hooked!");
    
    // Hook Update
    HOOK(targetLibName, RVA_ATTACK_COMPONENT_UPDATE, Update, old_Update);
    
    LOGI(OBFUSCATE("Hooks initialized!"));

#elif defined(__arm__)
    LOGI("ARM32 not supported for this game");
#endif

    LOGI(OBFUSCATE("School of Chaos Mod - Ready!"));
    return nullptr;
}

// ======================================
// Library Entry Point
// ======================================
__attribute__((constructor))
void lib_main() {
    pthread_t ptid;
    pthread_create(&ptid, NULL, hack_thread, NULL);
}
