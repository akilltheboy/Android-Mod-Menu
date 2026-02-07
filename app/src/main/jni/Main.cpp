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
        
        // Handle Item Spawning - SEARCH ALL ITEMS
        if (spawnItemPressed) {
            spawnItemPressed = false;
            LOGI("=== ITEM SPAWN ===");
            LOGI("Target item: %s", itemNameToSpawn.c_str());
            LOGI("All items array: %p, count: %d", g_AllItemsArray, g_AllItemsCount);
            
            void* foundItem = nullptr;
            
            // APPROACH 1: Search from g_AllItemsArray if available
            if (g_AllItemsArray != nullptr && g_AllItemsCount > 0 && Il2CppStringNew != nullptr) {
                void** allItems = (void**)((uintptr_t)g_AllItemsArray + 0x20);
                
                for (int i = 0; i < g_AllItemsCount && i < 2000; i++) {
                    void* item = allItems[i];
                    if (item == nullptr) continue;
                    
                    // Get item's GameObject name at offset 0x10 (UnityEngine.Object.m_CachedPtr)
                    // Actually MonoBehaviour/Component stores name differently
                    // BadNerdItem extends VNLObject which has obfuscated name
                    // Let's try comparing gameObject.name through Component.gameObject.name
                    
                    // For debugging, log first 5 items
                    if (i < 5) {
                        LOGI("Item[%d]: %p", i, item);
                    }
                    
                    // Try to get name - VNLObject has name via get_name() typically at some offset
                    // Try offset 0x10 which is commonly the name string in il2cpp
                    void* itemName = *(void**)((uintptr_t)item + 0x10);
                    if (itemName != nullptr) {
                        // Il2CppString: [0x10] = length, [0x14] = chars
                        int nameLen = *(int*)((uintptr_t)itemName + 0x10);
                        if (nameLen > 0 && nameLen < 100) {
                            char16_t* nameChars = (char16_t*)((uintptr_t)itemName + 0x14);
                            // Simple comparison for first chars
                            bool match = true;
                            for (size_t j = 0; j < itemNameToSpawn.length() && j < (size_t)nameLen; j++) {
                                if ((char)nameChars[j] != itemNameToSpawn[j]) {
                                    match = false;
                                    break;
                                }
                            }
                            if (match && itemNameToSpawn.length() <= (size_t)nameLen) {
                                foundItem = item;
                                LOGI("Found matching item at index %d: %p", i, item);
                                break;
                            }
                        }
                    }
                }
            }
            
            // APPROACH 2: Fallback to GetItemByName if item is in inventory
            if (foundItem == nullptr && GetItemByName != nullptr && Il2CppStringNew != nullptr) {
                void* nameStr = Il2CppStringNew(itemNameToSpawn.c_str());
                foundItem = GetItemByName(instance, nameStr);
                if (foundItem != nullptr) {
                    LOGI("Found item in inventory: %p", foundItem);
                }
            }
            
            // Add item to inventory
            if (foundItem != nullptr) {
                void* itemList = *(void**)((uintptr_t)instance + 0x138);
                LOGI("itemList: %p", itemList);
                
                if (itemList != nullptr) {
                    void* listItems = *(void**)((uintptr_t)itemList + 0x10);
                    int listSize = *(int*)((uintptr_t)itemList + 0x18);
                    
                    LOGI("List _items: %p, _size: %d", listItems, listSize);
                    
                    if (listItems != nullptr) {
                        int listCapacity = *(int*)((uintptr_t)listItems + 0x18);
                        LOGI("Array capacity: %d", listCapacity);
                        
                        if (listSize < listCapacity) {
                            void** listItemsArray = (void**)((uintptr_t)listItems + 0x20);
                            listItemsArray[listSize] = foundItem;
                            *(int*)((uintptr_t)itemList + 0x18) = listSize + 1;
                            LOGI("SUCCESS! Item added! New size: %d", listSize + 1);
                        } else {
                            LOGI("List is full! Size: %d, Capacity: %d", listSize, listCapacity);
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
    // Get all il2cpp API functions using dlsym
    void* il2cppHandle = dlopen("libil2cpp.so", RTLD_NOLOAD);
    if (il2cppHandle != nullptr) {
        Il2CppStringNew = (Il2CppStringNew_t)dlsym(il2cppHandle, "il2cpp_string_new");
        il2cpp_class_from_name = (il2cpp_class_from_name_t)dlsym(il2cppHandle, "il2cpp_class_from_name");
        il2cpp_class_get_type = (il2cpp_class_get_type_t)dlsym(il2cppHandle, "il2cpp_class_get_type");
        il2cpp_domain_get = (il2cpp_domain_get_t)dlsym(il2cppHandle, "il2cpp_domain_get");
        il2cpp_domain_get_assemblies = (il2cpp_domain_get_assemblies_t)dlsym(il2cppHandle, "il2cpp_domain_get_assemblies");
        il2cpp_assembly_get_image = (il2cpp_assembly_get_image_t)dlsym(il2cppHandle, "il2cpp_assembly_get_image");
        il2cpp_type_get_object = (il2cpp_type_get_object_t)dlsym(il2cppHandle, "il2cpp_type_get_object");
        
        LOGI("il2cpp_string_new: %p", Il2CppStringNew);
        LOGI("il2cpp_class_from_name: %p", il2cpp_class_from_name);
        LOGI("il2cpp_domain_get: %p", il2cpp_domain_get);
        
        // Get FindObjectsOfTypeAll from RVA
        FindObjectsOfTypeAll = (FindObjectsOfTypeAll_t)getAbsoluteAddress(targetLibName, 0x423CCE4);
        LOGI("FindObjectsOfTypeAll: %p", FindObjectsOfTypeAll);
        
        // Try to find BadNerdItem class
        if (il2cpp_domain_get != nullptr && il2cpp_domain_get_assemblies != nullptr) {
            void* domain = il2cpp_domain_get();
            LOGI("Il2Cpp Domain: %p", domain);
            
            if (domain != nullptr) {
                size_t assemblyCount = 0;
                void** assemblies = il2cpp_domain_get_assemblies(domain, &assemblyCount);
                LOGI("Assemblies count: %zu", assemblyCount);
                
                // Look for BadNerdItem in Assembly-CSharp
                for (size_t i = 0; i < assemblyCount && il2cpp_class_from_name != nullptr; i++) {
                    void* image = il2cpp_assembly_get_image(assemblies[i]);
                    if (image != nullptr) {
                        // Try to find BadNerdItem class (no namespace, root namespace)
                        void* badNerdItemClass = il2cpp_class_from_name(image, "", "BadNerdItem");
                        if (badNerdItemClass != nullptr) {
                            LOGI("Found BadNerdItem class: %p in assembly %zu", badNerdItemClass, i);
                            
                            // Get the Type object for FindObjectsOfTypeAll
                            if (il2cpp_class_get_type != nullptr && il2cpp_type_get_object != nullptr) {
                                void* type = il2cpp_class_get_type(badNerdItemClass);
                                if (type != nullptr) {
                                    void* typeObject = il2cpp_type_get_object(type);
                                    LOGI("BadNerdItem TypeObject: %p", typeObject);
                                    
                                    // Call FindObjectsOfTypeAll to get all items
                                    if (FindObjectsOfTypeAll != nullptr && typeObject != nullptr) {
                                        g_AllItemsArray = FindObjectsOfTypeAll(typeObject);
                                        if (g_AllItemsArray != nullptr) {
                                            g_AllItemsCount = *(int*)((uintptr_t)g_AllItemsArray + 0x18);
                                            LOGI("Found %d BadNerdItem objects!", g_AllItemsCount);
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
