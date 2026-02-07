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
typedef void* (*ListAdd_t)(void* list, void* item);

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
            OBFUSCATE("RichTextView_<b>Item Names Examples:</b><br/>• Spec-Ops Katana<br/>• Steel Bat<br/>• Big Hammer<br/>• Golden Fang<br/>• Happy Mace"),
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
            if (text != nullptr) {
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
            if (text != nullptr) {
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
        // Only save the instance, don't process yet
        if (g_AttackComponent == nullptr) {
            g_AttackComponent = instance;
            LOGI("AttackComponent captured: %p", instance);
        }
        
        // Handle Item Spawning - SAFE VERSION
        if (spawnItemPressed) {
            spawnItemPressed = false;
            LOGI("=== SPAWN ITEM DEBUG ===");
            LOGI("Instance: %p", instance);
            LOGI("Item name: %s", itemNameToSpawn.c_str());
            
            // Check if we can read itemList
            void* itemList = *(void**)((uintptr_t)instance + 0x138);
            LOGI("itemList pointer: %p", itemList);
            
            // Try to call GetItemByName safely
            if (GetItemByName != nullptr) {
                LOGI("GetItemByName function: %p", GetItemByName);
                // Don't call yet, just log
            }
            
            LOGI("=== END DEBUG ===");
        }
        
        // Handle Level Setting - SAFE VERSION
        if (setLevelPressed) {
            setLevelPressed = false;
            LOGI("=== LEVEL UP DEBUG ===");
            LOGI("Instance: %p", instance);
            LOGI("Level value: %s", levelValue.c_str());
            
            // Read current _level field
            void* currentLevel = *(void**)((uintptr_t)instance + 0x198);
            LOGI("Current _level pointer: %p", currentLevel);
            
            LOGI("=== END DEBUG ===");
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
#define RVA_IL2CPP_STRING_NEW    0x2FFE8C   // il2cpp_string_new
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
    uintptr_t il2cppBase = g_il2cppELF.base();
    
    // Get il2cpp_string_new function
    Il2CppStringNew = (Il2CppStringNew_t)getAbsoluteAddress(targetLibName, RVA_IL2CPP_STRING_NEW);
    LOGI("Il2CppStringNew at: %p", Il2CppStringNew);
    
    // Get GetItemByName function
    GetItemByName = (GetItemByName_t)getAbsoluteAddress(targetLibName, RVA_GET_ITEM_BY_NAME);
    LOGI("GetItemByName at: %p", GetItemByName);
    
    // Hook Update to capture player instance and handle features
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
