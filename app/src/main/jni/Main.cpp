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

Il2CppStringNew_t Il2CppStringNew = nullptr;
GetItemByName_t GetItemByName = nullptr;
FindObjectsOfTypeAll_t FindObjectsOfTypeAll = nullptr;
il2cpp_class_from_name_t il2cpp_class_from_name = nullptr;
il2cpp_class_get_type_t il2cpp_class_get_type = nullptr;
il2cpp_domain_get_t il2cpp_domain_get = nullptr;
il2cpp_domain_get_assemblies_t il2cpp_domain_get_assemblies = nullptr;
il2cpp_assembly_get_image_t il2cpp_assembly_get_image = nullptr;
il2cpp_type_get_object_t il2cpp_type_get_object = nullptr;


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
        
        // Handle Item Spawning - TRUE SPAWNER
        if (spawnItemPressed) {
            spawnItemPressed = false;
            LOGI("=== TRUE ITEM SPAWN ===");
            LOGI("Target item: %s", itemNameToSpawn.c_str());
            
            // LAZY LOAD: Get all BadNerdItems on first spawn (game must be loaded)
            if (g_AllItemsArray == nullptr && il2cpp_domain_get != nullptr) {
                LOGI("Loading all items for first time...");
                
                void* domain = il2cpp_domain_get();
                if (domain != nullptr) {
                    size_t assemblyCount = 0;
                    void** assemblies = il2cpp_domain_get_assemblies(domain, &assemblyCount);
                    LOGI("Found %zu assemblies", assemblyCount);
                    
                    for (size_t i = 0; i < assemblyCount; i++) {
                        void* image = il2cpp_assembly_get_image(assemblies[i]);
                        if (image != nullptr) {
                            void* badNerdItemClass = il2cpp_class_from_name(image, "", "BadNerdItem");
                            if (badNerdItemClass != nullptr) {
                                LOGI("Found BadNerdItem class: %p", badNerdItemClass);
                                
                                void* type = il2cpp_class_get_type(badNerdItemClass);
                                if (type != nullptr) {
                                    void* typeObject = il2cpp_type_get_object(type);
                                    LOGI("Type object: %p", typeObject);
                                    
                                    if (FindObjectsOfTypeAll != nullptr && typeObject != nullptr) {
                                        g_AllItemsArray = FindObjectsOfTypeAll(typeObject);
                                        if (g_AllItemsArray != nullptr) {
                                            g_AllItemsCount = *(int*)((uintptr_t)g_AllItemsArray + 0x18);
                                            LOGI("LOADED %d BadNerdItem objects!", g_AllItemsCount);
                                        }
                                    }
                                }
                                break;
                            }
                        }
                    }
                }
            }
            
            // Search for item in loaded items
            void* foundItem = nullptr;
            
            if (g_AllItemsArray != nullptr && g_AllItemsCount > 0) {
                LOGI("Searching %d items for: %s", g_AllItemsCount, itemNameToSpawn.c_str());
                void** allItems = (void**)((uintptr_t)g_AllItemsArray + 0x20);
                
                // DEBUG: Dump first 3 items' structure
                for (int i = 0; i < 3 && i < g_AllItemsCount; i++) {
                    void* item = allItems[i];
                    if (item == nullptr) continue;
                    
                    LOGI("=== ITEM[%d] at %p ===", i, item);
                    
                    // Try different offsets to find name string
                    int offsets[] = {0x10, 0x18, 0x20, 0x30, 0x40, 0x90};
                    for (int k = 0; k < 6; k++) {
                        void* ptr = *(void**)((uintptr_t)item + offsets[k]);
                        if (ptr != nullptr) {
                            // Try to read as Il2CppString
                            int len = *(int*)((uintptr_t)ptr + 0x10);
                            if (len > 0 && len < 100) {
                                char16_t* chars = (char16_t*)((uintptr_t)ptr + 0x14);
                                char name[64] = {0};
                                for (int j = 0; j < len && j < 63; j++) {
                                    name[j] = (char)chars[j];
                                }
                                LOGI("  [0x%X] len=%d: %s", offsets[k], len, name);
                            } else {
                                LOGI("  [0x%X] ptr=%p (len=%d invalid)", offsets[k], ptr, len);
                            }
                        }
                    }
                }
                
                // Now search all items using description offset 0x90
                for (int i = 0; i < g_AllItemsCount && i < 3000; i++) {
                    void* item = allItems[i];
                    if (item == nullptr) continue;
                    
                    // Try multiple offsets for name
                    int nameOffsets[] = {0x10, 0x18, 0x30, 0x90};
                    for (int k = 0; k < 4; k++) {
                        void* itemNameStr = *(void**)((uintptr_t)item + nameOffsets[k]);
                        if (itemNameStr == nullptr) continue;
                        
                        int nameLen = *(int*)((uintptr_t)itemNameStr + 0x10);
                        if (nameLen <= 0 || nameLen >= 100) continue;
                        
                        char16_t* nameChars = (char16_t*)((uintptr_t)itemNameStr + 0x14);
                        
                        // Check if matches (case insensitive partial match)
                        bool match = true;
                        if ((size_t)nameLen >= itemNameToSpawn.length()) {
                            for (size_t j = 0; j < itemNameToSpawn.length(); j++) {
                                char c1 = (char)nameChars[j];
                                char c2 = itemNameToSpawn[j];
                                if (c1 != c2 && (c1 ^ 32) != c2) {
                                    match = false;
                                    break;
                                }
                            }
                            if (match) {
                                char foundName[64] = {0};
                                for (int j = 0; j < nameLen && j < 63; j++) {
                                    foundName[j] = (char)nameChars[j];
                                }
                                LOGI("FOUND at [%d] offset 0x%X: %s", i, nameOffsets[k], foundName);
                                foundItem = item;
                                break;
                            }
                        }
                    }
                    if (foundItem) break;
                }
            } else {
                LOGI("No items loaded! g_AllItemsArray=%p, count=%d", g_AllItemsArray, g_AllItemsCount);
            }
            
            // Add item to player inventory
            if (foundItem != nullptr) {
                void* itemList = *(void**)((uintptr_t)instance + 0x138);
                LOGI("Adding to itemList: %p", itemList);
                
                if (itemList != nullptr) {
                    void* listItems = *(void**)((uintptr_t)itemList + 0x10);
                    int listSize = *(int*)((uintptr_t)itemList + 0x18);
                    
                    LOGI("Current size: %d", listSize);
                    
                    if (listItems != nullptr) {
                        int listCapacity = *(int*)((uintptr_t)listItems + 0x18);
                        LOGI("Current capacity: %d", listCapacity);
                        
                        // If full, try to expand capacity first
                        if (listSize >= listCapacity && listCapacity < 100) {
                            // Double the capacity
                            int newCapacity = listCapacity * 2;
                            if (newCapacity < 20) newCapacity = 20;
                            *(int*)((uintptr_t)listItems + 0x18) = newCapacity;
                            listCapacity = newCapacity;
                            LOGI("Expanded capacity to: %d", newCapacity);
                        }
                        
                        if (listSize < listCapacity) {
                            void** listItemsArray = (void**)((uintptr_t)listItems + 0x20);
                            listItemsArray[listSize] = foundItem;
                            *(int*)((uintptr_t)itemList + 0x18) = listSize + 1;
                            LOGI("SUCCESS! Item spawned! New size: %d", listSize + 1);
                        } else {
                            LOGI("Still full after expand! Size=%d, Cap=%d", listSize, listCapacity);
                        }
                    }
                }
            } else {
                LOGI("Item not found: %s", itemNameToSpawn.c_str());
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
