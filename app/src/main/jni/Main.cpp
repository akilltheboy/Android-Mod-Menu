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
        // Save the instance (this is PlayerAttackComponent extending AttackComponent)
        if (g_AttackComponent == nullptr) {
            g_AttackComponent = instance;
            LOGI("PlayerAttackComponent captured: %p", instance);
            
            // Log the master item array info
            void* masterItemArray = *(void**)((uintptr_t)instance + 0x6A0);
            if (masterItemArray != nullptr) {
                int arrayLength = *(int*)((uintptr_t)masterItemArray + 0x18);
                LOGI("Master Item Array: %p, Length: %d", masterItemArray, arrayLength);
            }
        }
        
        // Handle Item Spawning - NEW IMPLEMENTATION
        if (spawnItemPressed) {
            spawnItemPressed = false;
            LOGI("=== SPAWNING ITEM (NEW) ===");
            LOGI("Target item: %s", itemNameToSpawn.c_str());
            
            // Get master item array at offset 0x6A0 (PlayerAttackComponent)
            void* masterItemArray = *(void**)((uintptr_t)instance + 0x6A0);
            LOGI("Master Array: %p", masterItemArray);
            
            if (masterItemArray != nullptr) {
                // Il2Cpp Array: [0x18] = length, [0x20] = first element
                int arrayLength = *(int*)((uintptr_t)masterItemArray + 0x18);
                void** items = (void**)((uintptr_t)masterItemArray + 0x20);
                LOGI("Array length: %d", arrayLength);
                
                void* foundItem = nullptr;
                
                // Search for item by name
                for (int i = 0; i < arrayLength && i < 500; i++) {
                    void* item = items[i];
                    if (item == nullptr) continue;
                    
                    // Get item's GameObject name using Unity's Object.name
                    // MonoBehaviour has Component base which has gameObject
                    // Try to get name via ToString or similar
                    
                    // For now, try using GetItemByName to see if this item matches
                    if (Il2CppStringNew != nullptr && GetItemByName != nullptr) {
                        // Try to match - this is a workaround
                        void* nameStr = Il2CppStringNew(itemNameToSpawn.c_str());
                        void* matchedItem = GetItemByName(instance, nameStr);
                        
                        if (matchedItem != nullptr) {
                            // We found the item in inventory, now get its template from master array
                            // by comparing at same index range
                            foundItem = item;
                            LOGI("Found potential match at index %d: %p", i, item);
                            break;
                        }
                    }
                }
                
                // Alternative: Search master array and compare first few chars
                if (foundItem == nullptr) {
                    LOGI("Searching master array by first item...");
                    // Just log first 10 items for debugging
                    for (int i = 0; i < 10 && i < arrayLength; i++) {
                        void* item = items[i];
                        if (item != nullptr) {
                            LOGI("Item[%d]: %p", i, item);
                        }
                    }
                }
                
                // Try to add item to inventory list
                if (foundItem != nullptr) {
                    void* itemList = *(void**)((uintptr_t)instance + 0x138);
                    LOGI("Adding to itemList: %p", itemList);
                    
                    if (itemList != nullptr) {
                        // Il2Cpp List: [0x10] = _items array, [0x18] = _size
                        void* listItems = *(void**)((uintptr_t)itemList + 0x10);
                        int listSize = *(int*)((uintptr_t)itemList + 0x18);
                        int listCapacity = 0;
                        
                        if (listItems != nullptr) {
                            listCapacity = *(int*)((uintptr_t)listItems + 0x18);
                        }
                        
                        LOGI("List size: %d, capacity: %d", listSize, listCapacity);
                        
                        if (listSize < listCapacity && listItems != nullptr) {
                            // Add item directly to the list's items array
                            void** listItemsArray = (void**)((uintptr_t)listItems + 0x20);
                            listItemsArray[listSize] = foundItem;
                            *(int*)((uintptr_t)itemList + 0x18) = listSize + 1;
                            LOGI("Item added! New size: %d", listSize + 1);
                        } else {
                            LOGI("List is full or invalid");
                        }
                    }
                }
            }
            LOGI("=== END SPAWN ===");
        }
        
        // Handle Level Setting
        if (setLevelPressed && Il2CppStringNew != nullptr) {
            setLevelPressed = false;
            LOGI("=== SETTING LEVEL ===");
            
            // Create Il2Cpp string from level value
            void* levelStr = Il2CppStringNew(levelValue.c_str());
            LOGI("Created level string: %p", levelStr);
            
            if (levelStr != nullptr) {
                // Set _level field at offset 0x198
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
