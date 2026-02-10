// ======================================
// School of Chaos v1.891 - Protocol Migration
// Data Serialization Bug Fix - Inventory Synchronization
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

// ======================================
// Il2Cpp Type Definitions (Preserved)
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

// ======================================
// NetworkCore.GiveItem - FIXED SIGNATURE
// ======================================
// ANALYSIS RESULT (from RESEARCH_NOTES.md):
// 
// OLD v1.761 API (FAILED - Silent Server Rejection):
//   void GiveItem(string itemName, int count, string sender, string msg, Callback callback)
//   
// NEW v1.891 API (CORRECT - Object Injection):
//   void GiveItem(void* networkCore, BadNerdItem* itemObj, int count, 
//                 void* senderName, void* message, void* lastParam)
//
// CRITICAL CHANGE: Parameter 1 changed from String -> BadNerdItem* (Object Pointer)
// This requires: FindItemByName(String) -> Returns BadNerdItem* -> Pass to GiveItem
//
// === LAST PARAMETER MYSTERY - RESOLVED ===
// 
// SUSPICION: The last parameter might be a Validation Token/Checksum in v1.891
//            instead of the Callback from v1.761.
// 
// INVESTIGATION: Checked assembly logic around RVA 0x302EC74
//   - No calls to hash functions (CRC32, MD5, SHA) before pushing last argument
//   - No math operations suggesting checksum generation
//   - Pattern matches v1.761 callback parameter position
// 
// VERDICT: Pass NULL (nullptr)
//   - If Callback: NULL is valid (async fire-and-forget)
//   - If Validation Token: Server may reject; implement generator if needed
//   - No evidence in dump.cs of checksum validation for this method
// 
// OFFSET: 0x302EC74 (NetworkCore.GiveItem)
// ======================================
typedef void (*NetworkCoreGiveItem_t)(
    void* networkCore,    // NetworkCore instance
    void* badNerdItem,    // BadNerdItem* object (NOT string!)
    int count,            // Item quantity
    void* senderName,     // Sender string (e.g., "VNL Entertainment")
    void* message,        // Message string (usually empty)
    void* lastParam       // DECISION: NULL (see analysis above)
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
NetworkCoreGiveItem_t NetworkCoreGiveItem = nullptr;

// ======================================
// Feature List - OPTIMIZED (2 Options Only)
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
// Menu Changes Handler - Simplified
// ======================================
void Changes(JNIEnv *env, jclass clazz, jobject obj, jint featNum, jstring featName, 
             jint value, jlong Lvalue, jboolean boolean, jstring text) {
    
    const char *featureNameChar = env->GetStringUTFChars(featName, nullptr);
    const char *textChar = text != nullptr ? env->GetStringUTFChars(text, nullptr) : "";
    
    LOGI("Feature: %s, FeatNum: %d, Text: %s", featureNameChar, featNum, textChar);
    
    switch (featNum) {
        case 0: // Target Item Name Input
            if (text != nullptr && strlen(textChar) > 0) {
                targetItemName = textChar;
                LOGI("[PROTOCOL] Target item set: %s", targetItemName.c_str());
            }
            break;
            
        case 1: // Inject Item Button
            if (!targetItemName.empty()) {
                injectItemPressed = true;
                LOGI("[PROTOCOL] Inject requested for: %s", targetItemName.c_str());
            } else {
                LOGI("[PROTOCOL] ERROR: No item name specified!");
            }
            break;
    }
    
    if (text != nullptr) {
        env->ReleaseStringUTFChars(text, textChar);
    }
    env->ReleaseStringUTFChars(featName, featureNameChar);
}

// ======================================
// Find NetworkCore Instance
// ======================================
void* FindNetworkCoreInstance() {
    if (g_NetworkCore != nullptr) {
        return g_NetworkCore;
    }
    
    if (il2cpp_domain_get == nullptr || FindObjectsOfTypeAll == nullptr) {
        return nullptr;
    }
    
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
// CORRECT SERIALIZATION FLOW
// ======================================
// Step 1: FindItemByName(String name) -> Returns BadNerdItem*
// Step 2: NetworkCore.GiveItem(BadNerdItem*, count, sender, msg, NULL)
// ======================================
bool InjectItem(const std::string& itemName) {
    LOGI("==============================================");
    LOGI("[PROTOCOL] Starting Object Injection Sequence");
    LOGI("[PROTOCOL] Target: %s", itemName.c_str());
    LOGI("==============================================");
    
    // Validate prerequisites
    if (g_AttackComponent == nullptr) {
        LOGE("[PROTOCOL] ERROR: AttackComponent not captured!");
        return false;
    }
    
    if (GetItemByName == nullptr || Il2CppStringNew == nullptr) {
        LOGE("[PROTOCOL] ERROR: Il2Cpp functions not initialized!");
        return false;
    }
    
    // STEP 1: Get BadNerdItem* object from name
    LOGI("[PROTOCOL] Step 1: Resolving BadNerdItem* from name...");
    void* itemNameStr = Il2CppStringNew(itemName.c_str());
    if (itemNameStr == nullptr) {
        LOGE("[PROTOCOL] ERROR: Failed to create Il2CppString!");
        return false;
    }
    
    void* itemObj = GetItemByName(g_AttackComponent, itemNameStr);
    if (itemObj == nullptr) {
        LOGE("[PROTOCOL] ERROR: Item '%s' not found!", itemName.c_str());
        LOGI("[PROTOCOL] TIP: Use exact item names from game files");
        return false;
    }
    LOGI("[PROTOCOL] Step 1 SUCCESS: BadNerdItem* = %p", itemObj);
    
    // STEP 2: Get NetworkCore instance
    LOGI("[PROTOCOL] Step 2: Acquiring NetworkCore instance...");
    void* networkCore = FindNetworkCoreInstance();
    if (networkCore == nullptr) {
        LOGE("[PROTOCOL] ERROR: NetworkCore not found!");
        LOGE("[PROTOCOL] TIP: Ensure connected to game server");
        return false;
    }
    LOGI("[PROTOCOL] Step 2 SUCCESS: NetworkCore = %p", networkCore);
    
    // STEP 3: Validate NetworkCoreGiveItem function
    if (NetworkCoreGiveItem == nullptr) {
        LOGE("[PROTOCOL] ERROR: NetworkCore.GiveItem not initialized!");
        return false;
    }
    
    // STEP 4: Prepare parameters for server call
    LOGI("[PROTOCOL] Step 3: Preparing server request...");
    void* senderStr = Il2CppStringNew("VNL Entertainment");
    void* messageStr = Il2CppStringNew("");
    int count = 1;
    
    LOGI("[PROTOCOL] Parameters:");
    LOGI("  - NetworkCore: %p", networkCore);
    LOGI("  - BadNerdItem: %p (%s)", itemObj, itemName.c_str());
    LOGI("  - Count: %d", count);
    LOGI("  - Sender: VNL Entertainment");
    LOGI("  - Message: (empty)");
    LOGI("  - LastParam: NULL (callback/validation - see header comment)");
    
    // STEP 5: Execute server-side item injection
    LOGI("[PROTOCOL] Step 4: Calling NetworkCore.GiveItem...");
    LOGI("[PROTOCOL] NOTE: Using Object API (v1.891) - NOT String API (v1.761)");
    
    NetworkCoreGiveItem(networkCore, itemObj, count, senderStr, messageStr, nullptr);
    
    LOGI("==============================================");
    LOGI("[PROTOCOL] Injection request SENT to server!");
    LOGI("[PROTOCOL] Check inbox for: %s", itemName.c_str());
    LOGI("==============================================");
    
    return true;
}

// ======================================
// Hook: Update (Capture AttackComponent)
// ======================================
void (*old_Update)(void *instance);
void Update(void *instance) {
    if (instance != nullptr) {
        // Capture AttackComponent instance (needed for GetItemByName)
        if (g_AttackComponent == nullptr) {
            g_AttackComponent = instance;
            LOGI("[PROTOCOL] AttackComponent captured: %p", instance);
        }
        
        // Handle Item Injection Request
        if (injectItemPressed) {
            injectItemPressed = false;
            InjectItem(targetItemName);
        }
    }
    
    return old_Update(instance);
}

// ======================================
// Target Library & RVA Offsets
// ======================================
#define targetLibName OBFUSCATE("libil2cpp.so")

ElfScanner g_il2cppELF;

// RVA Offsets for School of Chaos v1.891 (arm64)
// SOURCE: RESEARCH_NOTES.md - Diff Report v1.761 -> v1.891
#define RVA_ATTACK_COMPONENT_UPDATE  0x41CB458   // AttackComponent.Update()
#define RVA_GET_ITEM_BY_NAME         0x41C8D24   // AttackComponent.GetItemByName(string)
#define RVA_NETWORKCORE_GIVE_ITEM    0x302EC74   // NetworkCore.GiveItem(BadNerdItem*, ...)

// ======================================
// Hack Thread (Initialization)
// ======================================
void *hack_thread(void *) {
    LOGI(OBFUSCATE("========================================"));
    LOGI(OBFUSCATE("School of Chaos v1.891 - Protocol Migration"));
    LOGI(OBFUSCATE("Data Serialization Bug Fix"));
    LOGI(OBFUSCATE("========================================"));

    // Wait for libil2cpp.so to load
    do {
        sleep(1);
        g_il2cppELF = ElfScanner::createWithPath(targetLibName);
    } while (!g_il2cppELF.isValid());

    LOGI(OBFUSCATE("%s loaded at base 0x%llX"), 
         (const char *)targetLibName, (long long)g_il2cppELF.base());

#if defined(__aarch64__)
    // Initialize Il2Cpp API via dlsym
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
    } else {
        LOGE("[INIT] Failed to get libil2cpp.so handle!");
    }
    
    // Initialize FindObjectsOfTypeAll
    FindObjectsOfTypeAll = (FindObjectsOfTypeAll_t)getAbsoluteAddress(targetLibName, 0x423CCE4);
    LOGI("[INIT] FindObjectsOfTypeAll: %p", FindObjectsOfTypeAll);
    
    // Initialize GetItemByName (for BadNerdItem* resolution)
    GetItemByName = (GetItemByName_t)getAbsoluteAddress(targetLibName, RVA_GET_ITEM_BY_NAME);
    LOGI("[INIT] GetItemByName: %p", GetItemByName);
    
    // Initialize NetworkCore.GiveItem (FIXED - Object API)
    NetworkCoreGiveItem = (NetworkCoreGiveItem_t)getAbsoluteAddress(targetLibName, RVA_NETWORKCORE_GIVE_ITEM);
    LOGI("[INIT] NetworkCore.GiveItem: %p", NetworkCoreGiveItem);
    LOGI("[INIT] API Version: v1.891 (Object-based)");
    
    // Hook Update to capture AttackComponent and handle injection
    HOOK(targetLibName, RVA_ATTACK_COMPONENT_UPDATE, Update, old_Update);
    LOGI("[INIT] AttackComponent.Update hooked");
    
    LOGI(OBFUSCATE("========================================"));
    LOGI(OBFUSCATE("Protocol Migration Ready"));
    LOGI(OBFUSCATE("Use: Input item name -> Press Inject"));
    LOGI(OBFUSCATE("========================================"));

#elif defined(__arm__)
    LOGE("[INIT] ARM32 not supported for this game version");
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
