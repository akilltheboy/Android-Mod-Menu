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
void* g_AttackComponent = nullptr;

// ======================================
// Il2Cpp Type Definitions
// ======================================
typedef void* (*Il2CppStringNew_t)(const char* text);
typedef void* (*GetItemByName_t)(void* instance, void* itemName);
typedef void (*ListAdd_t)(void* list, void* item);

Il2CppStringNew_t Il2CppStringNew = nullptr;
GetItemByName_t GetItemByName = nullptr;

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
            
            // Level Up
            OBFUSCATE("Collapse_⬆️ Level Up"),
            OBFUSCATE("CollapseAdd_InputText_Level Value"),
            OBFUSCATE("CollapseAdd_Button_🚀 Set Level"),
            
            // Info
            OBFUSCATE("Category_ℹ️ Info"),
            OBFUSCATE("RichTextView_<b>Item Names:</b><br/>• Energy Drink<br/>• Apple<br/>• Steel Bat<br/>• Spec-Ops Katana"),
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
            
        case 2: // Level Value Input
            if (text != nullptr && strlen(textChar) > 0) {
                levelValue = textChar;
                LOGI("Level value set to: %s", levelValue.c_str());
            }
            break;
            
        case 3: // Set Level Button
            if (!levelValue.empty()) {
                setLevelPressed = true;
                LOGI("Set level pressed: %s", levelValue.c_str());
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
        
        // Handle Item Spawning - DUPLICATION APPROACH
        if (spawnItemPressed) {
            spawnItemPressed = false;
            LOGI("=== ITEM DUPLICATION ===");
            LOGI("Target item: %s", itemNameToSpawn.c_str());
            
            // First, find the item in current inventory
            if (Il2CppStringNew != nullptr && GetItemByName != nullptr) {
                void* nameStr = Il2CppStringNew(itemNameToSpawn.c_str());
                void* foundItem = GetItemByName(instance, nameStr);
                
                LOGI("GetItemByName result: %p", foundItem);
                
                if (foundItem != nullptr) {
                    // Found the item! Now add it to inventory list
                    void* itemList = *(void**)((uintptr_t)instance + 0x138);
                    LOGI("itemList: %p", itemList);
                    
                    if (itemList != nullptr) {
                        // Il2Cpp List<T>: 
                        // [0x10] = _items (array)
                        // [0x18] = _size
                        void* listItems = *(void**)((uintptr_t)itemList + 0x10);
                        int listSize = *(int*)((uintptr_t)itemList + 0x18);
                        
                        LOGI("List _items: %p, _size: %d", listItems, listSize);
                        
                        if (listItems != nullptr) {
                            // Get array capacity from the array
                            int listCapacity = *(int*)((uintptr_t)listItems + 0x18);
                            LOGI("Array capacity: %d", listCapacity);
                            
                            if (listSize < listCapacity) {
                                // Add item to the array
                                void** listItemsArray = (void**)((uintptr_t)listItems + 0x20);
                                listItemsArray[listSize] = foundItem;
                                
                                // Increment size
                                *(int*)((uintptr_t)itemList + 0x18) = listSize + 1;
                                
                                LOGI("SUCCESS! Item duplicated! New size: %d", listSize + 1);
                            } else {
                                LOGI("List is full! Size: %d, Capacity: %d", listSize, listCapacity);
                            }
                        }
                    }
                } else {
                    LOGI("Item not in inventory! You need to have the item first.");
                }
            }
            LOGI("=== END DUPLICATION ===");
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
    // Get il2cpp_string_new using dlsym
    void* il2cppHandle = dlopen("libil2cpp.so", RTLD_NOLOAD);
    if (il2cppHandle != nullptr) {
        Il2CppStringNew = (Il2CppStringNew_t)dlsym(il2cppHandle, "il2cpp_string_new");
        LOGI("Il2CppStringNew (dlsym): %p", Il2CppStringNew);
    } else {
        LOGE("Failed to get libil2cpp.so handle");
    }
    
    // Get GetItemByName function
    GetItemByName = (GetItemByName_t)getAbsoluteAddress(targetLibName, RVA_GET_ITEM_BY_NAME);
    LOGI("GetItemByName at: %p", GetItemByName);
    
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
