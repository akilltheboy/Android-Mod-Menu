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
il2cpp_object_new_t il2cpp_object_new = nullptr;
void* g_NetworkCore = nullptr;
void* g_SmartFox = nullptr;
void* g_ExtensionRequestClass = nullptr;


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
    }
    
    if (text != nullptr) {
        env->ReleaseStringUTFChars(text, textChar);
    }
    env->ReleaseStringUTFChars(featName, featureNameChar);
}

// ======================================
// Hook Functions
// ======================================

// Hook Update to capture AttackComponent instance
void (*old_Update)(void *instance);
void Update(void *instance) {
    if (instance != nullptr) {
        // Save the instance
        if (g_AttackComponent == nullptr) {
            g_AttackComponent = instance;
            LOGI("PlayerAttackComponent captured: %p", instance);
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
        
        // Handle List All Items
        if (listItemsPressed && il2cpp_domain_get != nullptr) {
            listItemsPressed = false;
            LOGI("=== LISTING ALL ITEMS ===");
            
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
                                        LOGI("=== TOTAL ITEMS LOADED: %d ===", count);
                                        LOGI("Use exact item names from the game!");
                                        LOGI("Example names: VIP Token, Energy Drink, Apple");
                                    }
                                }
                            }
                            break;
                        }
                    }
                }
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
            
            // Find ExtensionRequest class and allocate object
            if (g_ExtensionRequestClass == nullptr) {
                void* domain = il2cpp_domain_get();
                if (domain != nullptr) {
                    size_t assembliesCount = 0;
                    void** assemblies = il2cpp_domain_get_assemblies(domain, &assembliesCount);
                    
                    for (size_t i = 0; i < assembliesCount; i++) {
                        void* image = il2cpp_assembly_get_image(assemblies[i]);
                        if (image == nullptr) continue;
                        
                        // ExtensionRequest is in Sfs2X.Requests namespace
                        void* extReqClass = il2cpp_class_from_name(image, "Sfs2X.Requests", "ExtensionRequest");
                        if (extReqClass != nullptr) {
                            g_ExtensionRequestClass = extReqClass;
                            LOGI("Found ExtensionRequest class: %p", g_ExtensionRequestClass);
                            break;
                        }
                    }
                }
            }
            
            if (g_ExtensionRequestClass != nullptr && il2cpp_object_new != nullptr && ExtensionRequest_ctor != nullptr) {
                // Allocate new ExtensionRequest object
                void* extRequest = il2cpp_object_new(g_ExtensionRequestClass);
                LOGI("Allocated ExtensionRequest: %p", extRequest);
                
                if (extRequest != nullptr) {
                    // Call constructor: ExtensionRequest(string cmd, ISFSObject params)
                    // params can be null for simple commands
                    ExtensionRequest_ctor(extRequest, cmdStr, nullptr);
                    LOGI("ExtensionRequest initialized with command: %s", serverCommand.c_str());
                    
                    // Send via SmartFox!
                    LOGI("Calling SmartFox.Send...");
                    SmartFox_Send(g_SmartFox, extRequest);
                    LOGI("=== COMMAND SENT TO SERVER! ===");
                }
            } else {
                if (g_ExtensionRequestClass == nullptr) LOGE("ExtensionRequest class not found!");
                if (il2cpp_object_new == nullptr) LOGE("il2cpp_object_new not available!");
            }
        } else {
            if (g_SmartFox == nullptr) LOGE("SmartFox not found!");
            if (SmartFox_Send == nullptr) LOGE("SmartFox_Send not initialized!");
        }
        
        LOGI("=== END SEND COMMAND ===");
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
    
    // Get il2cpp_object_new via dlsym for allocating new objects
    if (il2cppHandle != nullptr) {
        il2cpp_object_new = (il2cpp_object_new_t)dlsym(il2cppHandle, "il2cpp_object_new");
        LOGI("il2cpp_object_new at: %p", il2cpp_object_new);
    }
    
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
