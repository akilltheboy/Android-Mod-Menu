// ======================================
// School of Chaos v1.891 - Protocol Migration
// Data Serialization Bug Fix - Inventory Synchronization
// FACTORY PATTERN IMPLEMENTED - Blueprint → Instance
// 
// REFERENCE: RESEARCH_NOTES.md (Diff Report v1.761 -> v1.891)
// MIGRATION: String API -> Object API
// ======================================

#include <cstring>
#include <string>
#include <jni.h>
#include <unistd.h>
#include <dlfcn.h>
#include <pthread.h>
#include "Includes/Logger.h"
#include "Includes/obfuscate.h"
#include "Includes/Utils.hpp"
#include "Menu/Menu.hpp"
#include "Menu/Jni.hpp"
#include "Includes/Macros.h"

// ======================================
// Protocol Migration State
// ======================================
std::string targetItemName = "";
bool injectItemPressed = false;
void* g_AttackComponent = nullptr;
void* g_NetworkCore = nullptr;
void* g_InvGameItemClass = nullptr;  // Cache for InvGameItem class

// ======================================
// Il2Cpp Type Definitions
// ======================================
typedef void* (*Il2CppStringNew_t)(const char* text);
typedef void* (*GetItemByName_t)(void* instance, void* itemName);
typedef void* (*FindObjectsOfTypeAll_t)(void* type);
typedef void* (*il2cpp_class_from_name_t)(void* image, const char* namespaze, const char* name);
typedef void* (*il2cpp_class_get_type_t)(void* klass);
typedef void* (*il2cpp_domain_get_t)();
typedef void** (*il2cpp_domain_get_assemblies_t)(void* domain, size_t* size);
typedef void* (*il2cpp_assembly_get_image_t)(void* assembly);
typedef void* (*il2cpp_type_get_object_t)(void* type);
typedef void* (*il2cpp_object_new_t)(void* klass);

// Factory Constructor: InvGameItem.ctor(int itemId, InvBaseItem blueprint)
// RVA: 0x4775374 - Creates Instance from Blueprint
typedef void (*InvGameItemConstructor_t)(void* instance, int itemId, void* blueprint);

// NetworkCore.GiveItem
typedef void (*NetworkCoreGiveItem_t)(
    void* networkCore,
    void* badNerdItem,
    int count,
    void* senderName,
    void* message,
    void* lastParam
);

// ======================================
// Global Function Pointers
// ======================================
Il2CppStringNew_t Il2CppStringNew = nullptr;
GetItemByName_t GetItemByName = nullptr;
FindObjectsOfTypeAll_t FindObjectsOfTypeAll = nullptr;
il2cpp_class_from_name_t il2cpp_class_from_name = nullptr;
il2cpp_class_get_type_t il2cpp_class_get_type = nullptr;
il2cpp_domain_get_t il2cpp_domain_get = nullptr;
il2cpp_domain_get_assemblies_t il2cpp_domain_get_assemblies = nullptr;
il2cpp_assembly_get_image_t il2cpp_assembly_get_image = nullptr;
il2cpp_type_get_object_t il2cpp_type_get_object = nullptr;
il2cpp_object_new_t il2cpp_object_new = nullptr;
InvGameItemConstructor_t InvGameItemConstructor = nullptr;
NetworkCoreGiveItem_t NetworkCoreGiveItem = nullptr;

// ======================================
// Feature List
// ======================================
jobjectArray GetFeatureList(JNIEnv *env, jobject context) {
    jobjectArray ret;
    const char *features[] = {
        OBFUSCATE("Category_Protocol Migration (v1.891)"),
        OBFUSCATE("InputText_Target Item Name"),
        OBFUSCATE("Button_Inject Item (GiveItem)")
    };
    int Total_Feature = (sizeof features / sizeof features[0]);
    ret = (jobjectArray)
        env->NewObjectArray(Total_Feature, env->FindClass(OBFUSCATE("java/lang/String")),
                            env->NewStringUTF(""));
    for (int i = 0; i < Total_Feature; i++)
        env->SetObjectArrayElement(ret, i, env->NewStringUTF(features[i]));
    return ret;
}

// ======================================
// Menu Changes Handler
// ======================================
void Changes(JNIEnv *env, jclass clazz, jobject obj, jint featNum, jstring featName, 
             jint value, jlong Lvalue, jboolean boolean, jstring text) {
    const char *featureNameChar = env->GetStringUTFChars(featName, nullptr);
    const char *textChar = text != nullptr ? env->GetStringUTFChars(text, nullptr) : "";
    LOGI("Feature: %s, FeatNum: %d, Text: %s", featureNameChar, featNum, textChar);
    
    switch (featNum) {
        case 0:
            if (text != nullptr && strlen(textChar) > 0) {
                targetItemName = textChar;
                LOGI("[PROTOCOL] Target item set: %s", targetItemName.c_str());
            }
            break;
        case 1:
            if (!targetItemName.empty()) {
                injectItemPressed = true;
                LOGI("[PROTOCOL] Inject requested for: %s", targetItemName.c_str());
            }
            break;
    }
    
    if (text != nullptr) env->ReleaseStringUTFChars(text, textChar);
    env->ReleaseStringUTFChars(featName, featureNameChar);
}

// ======================================
// Find NetworkCore Instance
// ======================================
void* FindNetworkCoreInstance() {
    if (g_NetworkCore != nullptr) return g_NetworkCore;
    if (il2cpp_domain_get == nullptr || FindObjectsOfTypeAll == nullptr) return nullptr;
    
    void* domain = il2cpp_domain_get();
    if (domain == nullptr) return nullptr;
    
    size_t assemblyCount = 0;
    void** assemblies = il2cpp_domain_get_assemblies(domain, &assemblyCount);
    
    for (size_t i = 0; i < assemblyCount; i++) {
        void* image = il2cpp_assembly_get_image(assemblies[i]);
        if (image == nullptr) continue;
        
        void* networkCoreClass = il2cpp_class_from_name(image, "", "NetworkCore");
        if (networkCoreClass != nullptr) {
            void* type = il2cpp_class_get_type(networkCoreClass);
            if (type != nullptr) {
                void* typeObject = il2cpp_type_get_object(type);
                if (typeObject != nullptr) {
                    void* instancesArray = FindObjectsOfTypeAll(typeObject);
                    if (instancesArray != nullptr) {
                        int count = *(int*)((uintptr_t)instancesArray + 0x18);
                        if (count > 0) {
                            void** items = (void**)((uintptr_t)instancesArray + 0x20);
                            g_NetworkCore = items[0];
                            LOGI("[PROTOCOL] NetworkCore instance: %p", g_NetworkCore);
                            return g_NetworkCore;
                        }
                    }
                }
            }
            break;
        }
    }
    return nullptr;
}

// ======================================
// FACTORY: Create Item Instance from Blueprint
// ======================================
// Step 2 of 3: Convert Blueprint → Instance using constructor at 0x4775374
void* CreateItemInstance(void* blueprint) {
    if (blueprint == nullptr || il2cpp_object_new == nullptr || g_InvGameItemClass == nullptr) {
        LOGE("[FACTORY] Invalid parameters for instance creation!");
        return nullptr;
    }
    
    if (InvGameItemConstructor == nullptr) {
        LOGE("[FACTORY] Constructor not initialized!");
        return nullptr;
    }
    
    // Allocate new InvGameItem instance
    void* instance = il2cpp_object_new(g_InvGameItemClass);
    if (instance == nullptr) {
        LOGE("[FACTORY] Failed to allocate InvGameItem instance!");
        return nullptr;
    }
    
    // Get item ID from blueprint (offset 0x18 typically)
    int itemId = *(int*)((uintptr_t)blueprint + 0x18);
    LOGI("[FACTORY] Creating instance with itemId: %d", itemId);
    
    // Call constructor: InvGameItem.ctor(itemId, blueprint)
    // RVA: 0x4775374
    InvGameItemConstructor(instance, itemId, blueprint);
    
    LOGI("[FACTORY] Instance created: %p", instance);
    return instance;
}

// ======================================
// Inject Item with Factory Pattern
// ======================================
// Step 1: Get Blueprint (InvDatabase.GetItemByName)
// Step 2: Create Instance (Factory Constructor)
// Step 3: Send to Server (NetworkCore.GiveItem)
bool InjectItem(const std::string& itemName) {
    LOGI("==============================================");
    LOGI("[PROTOCOL] Starting Object Injection Sequence");
    LOGI("[PROTOCOL] Target: %s", itemName.c_str());
    LOGI("==============================================");
    
    if (g_AttackComponent == nullptr) {
        LOGE("[PROTOCOL] ERROR: AttackComponent not captured!");
        return false;
    }
    
    if (GetItemByName == nullptr || Il2CppStringNew == nullptr) {
        LOGE("[PROTOCOL] ERROR: Functions not initialized!");
        return false;
    }
    
    // STEP 1: Get Blueprint from global database
    LOGI("[PROTOCOL] Step 1: Getting Blueprint from InvDatabase...");
    void* itemNameStr = Il2CppStringNew(itemName.c_str());
    if (itemNameStr == nullptr) {
        LOGE("[PROTOCOL] ERROR: Failed to create Il2CppString!");
        return false;
    }
    
    void* blueprint = GetItemByName(nullptr, itemNameStr);  // Static method
    if (blueprint == nullptr) {
        LOGE("[PROTOCOL] ERROR: Item '%s' not found in database!", itemName.c_str());
        return false;
    }
    LOGI("[PROTOCOL] Step 1 SUCCESS: Blueprint = %p", blueprint);
    
    // STEP 2: Create Instance using Factory
    LOGI("[PROTOCOL] Step 2: Creating Instance from Blueprint...");
    void* instance = CreateItemInstance(blueprint);
    if (instance == nullptr) {
        LOGE("[PROTOCOL] ERROR: Failed to create item instance!");
        return false;
    }
    LOGI("[PROTOCOL] Step 2 SUCCESS: Instance = %p", instance);
    
    // STEP 3: Get NetworkCore and send
    LOGI("[PROTOCOL] Step 3: Sending to server...");
    void* networkCore = FindNetworkCoreInstance();
    if (networkCore == nullptr) {
        LOGE("[PROTOCOL] ERROR: NetworkCore not found!");
        return false;
    }
    
    if (NetworkCoreGiveItem == nullptr) {
        LOGE("[PROTOCOL] ERROR: GiveItem not initialized!");
        return false;
    }
    
    void* senderStr = Il2CppStringNew("VNL Entertainment");
    void* messageStr = Il2CppStringNew("");
    
    LOGI("[PROTOCOL] Calling GiveItem with Instance (NOT Blueprint)!");
    NetworkCoreGiveItem(networkCore, instance, 1, senderStr, messageStr, nullptr);
    
    LOGI("==============================================");
    LOGI("[PROTOCOL] Injection request SENT!");
    LOGI("==============================================");
    
    return true;
}

// ======================================
// Hook: Update
// ======================================
void (*old_Update)(void *instance);
void Update(void *instance) {
    if (instance != nullptr) {
        if (g_AttackComponent == nullptr) {
            g_AttackComponent = instance;
            LOGI("[PROTOCOL] AttackComponent captured: %p", instance);
        }
        if (injectItemPressed) {
            injectItemPressed = false;
            InjectItem(targetItemName);
        }
    }
    return old_Update(instance);
}

// ======================================
// RVA Offsets
// ======================================
#define targetLibName OBFUSCATE("libil2cpp.so")
ElfScanner g_il2cppELF;

#define RVA_ATTACK_COMPONENT_UPDATE  0x41CB458
#define RVA_GET_ITEM_BY_NAME         0x4772328   // InvDatabase.GetItemByName(string)
#define RVA_NETWORKCORE_GIVE_ITEM    0x302EC74   // NetworkCore.GiveItem
#define RVA_ITEM_CONSTRUCTOR         0x4775374   // InvGameItem.ctor(int, InvBaseItem)

// ======================================
// Hack Thread
// ======================================
void *hack_thread(void *) {
    LOGI(OBFUSCATE("========================================"));
    LOGI(OBFUSCATE("Protocol Migration v1.891 - FACTORY MODE"));
    LOGI(OBFUSCATE("========================================"));

    do {
        sleep(1);
        g_il2cppELF = ElfScanner::createWithPath(targetLibName);
    } while (!g_il2cppELF.isValid());

    LOGI(OBFUSCATE("%s loaded at base 0x%llX"), (const char *)targetLibName, (long long)g_il2cppELF.base());

#if defined(__aarch64__)
    void* il2cppHandle = dlopen("libil2cpp.so", RTLD_NOLOAD);
    if (il2cppHandle != nullptr) {
        Il2CppStringNew = (Il2CppStringNew_t)dlsym(il2cppHandle, "il2cpp_string_new");
        il2cpp_class_from_name = (il2cpp_class_from_name_t)dlsym(il2cppHandle, "il2cpp_class_from_name");
        il2cpp_class_get_type = (il2cpp_class_get_type_t)dlsym(il2cppHandle, "il2cpp_class_get_type");
        il2cpp_domain_get = (il2cpp_domain_get_t)dlsym(il2cppHandle, "il2cpp_domain_get");
        il2cpp_domain_get_assemblies = (il2cpp_domain_get_assemblies_t)dlsym(il2cppHandle, "il2cpp_domain_get_assemblies");
        il2cpp_assembly_get_image = (il2cpp_assembly_get_image_t)dlsym(il2cppHandle, "il2cpp_assembly_get_image");
        il2cpp_type_get_object = (il2cpp_type_get_object_t)dlsym(il2cppHandle, "il2cpp_type_get_object");
        il2cpp_object_new = (il2cpp_object_new_t)dlsym(il2cppHandle, "il2cpp_object_new");
        LOGI("[INIT] Il2Cpp API initialized");
    }
    
    // Initialize functions
    FindObjectsOfTypeAll = (FindObjectsOfTypeAll_t)getAbsoluteAddress(targetLibName, 0x423CCE4);
    GetItemByName = (GetItemByName_t)getAbsoluteAddress(targetLibName, RVA_GET_ITEM_BY_NAME);
    NetworkCoreGiveItem = (NetworkCoreGiveItem_t)getAbsoluteAddress(targetLibName, RVA_NETWORKCORE_GIVE_ITEM);
    InvGameItemConstructor = (InvGameItemConstructor_t)getAbsoluteAddress(targetLibName, RVA_ITEM_CONSTRUCTOR);
    
    LOGI("[INIT] GetItemByName: %p", GetItemByName);
    LOGI("[INIT] GiveItem: %p", NetworkCoreGiveItem);
    LOGI("[INIT] ItemConstructor: %p", InvGameItemConstructor);
    
    // Cache InvGameItem class for factory
    if (il2cpp_domain_get != nullptr) {
        void* domain = il2cpp_domain_get();
        if (domain != nullptr) {
            size_t assemblyCount = 0;
            void** assemblies = il2cpp_domain_get_assemblies(domain, &assemblyCount);
            for (size_t i = 0; i < assemblyCount; i++) {
                void* image = il2cpp_assembly_get_image(assemblies[i]);
                if (image != nullptr) {
                    g_InvGameItemClass = il2cpp_class_from_name(image, "", "InvGameItem");
                    if (g_InvGameItemClass != nullptr) {
                        LOGI("[INIT] InvGameItem class cached: %p", g_InvGameItemClass);
                        break;
                    }
                }
            }
        }
    }
    
    HOOK(targetLibName, RVA_ATTACK_COMPONENT_UPDATE, Update, old_Update);
    LOGI("[INIT] AttackComponent.Update hooked");
    LOGI(OBFUSCATE("========================================"));
    LOGI(OBFUSCATE("FACTORY PATTERN READY - Input & Inject"));
    LOGI(OBFUSCATE("========================================"));
#endif
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
