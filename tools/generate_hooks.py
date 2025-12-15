#!/usr/bin/env python3

import yaml
import json
import os
import sys
from pathlib import Path

def parse_detours_yaml(yaml_path):
    with open(yaml_path, 'r') as f:
        data = yaml.safe_load(f)
    return {func['name']: func for func in data.get('functions', [])}

def find_hooks_json_files(features_dir, core_dir):
    hooks_files = []
    for root, dirs, files in os.walk(features_dir):
        if 'hooks.json' in files:
            rel_path = os.path.relpath(root, features_dir)
            path_parts = rel_path.split(os.sep)
            feature_name = path_parts[0] if path_parts else os.path.basename(root)
            hooks_path = os.path.join(root, 'hooks.json')
            hooks_files.append((feature_name, hooks_path))
    if os.path.exists(os.path.join(core_dir, 'hooks.json')):
        hooks_path = os.path.join(core_dir, 'hooks.json')
        hooks_files.append(('core', hooks_path))
    return hooks_files


def find_callbacks_json_files(features_dir, core_dir):
    callbacks_files = []
    for root, dirs, files in os.walk(features_dir):
        if 'callbacks.json' in files:
            rel_path = os.path.relpath(root, features_dir)
            path_parts = rel_path.split(os.sep)
            feature_name = path_parts[0] if path_parts else os.path.basename(root)
            callbacks_path = os.path.join(root, 'callbacks.json')
            callbacks_files.append((feature_name, callbacks_path))
    if os.path.exists(os.path.join(core_dir, 'callbacks.json')):
        callbacks_path = os.path.join(core_dir, 'callbacks.json')
        callbacks_files.append(('core', callbacks_path))
    return callbacks_files

def parse_callbacks_json(callbacks_path):
    with open(callbacks_path, 'r') as f:
        return json.load(f)

def parse_features_txt(features_txt_path):
    enabled_features = set()
    if not os.path.exists(features_txt_path):
        return enabled_features
    with open(features_txt_path, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            if line.startswith('//'):
                continue
            enabled_features.add(line)
    return enabled_features


def parse_hooks_json(hooks_path):
    with open(hooks_path, 'r') as f:
        return json.load(f)

def find_used_detours(feature_hooks, src_dir, all_detours, enabled_features):
    used_detours = set()
    override_hooks = {}  # func_name -> (feature_name, callback_name)
    
    # First pass: find all override hooks
    for feature_name, hooks_path in feature_hooks:
        try:
            hooks = parse_hooks_json(hooks_path)
            for hook in hooks:
                func_name = hook['function']
                used_detours.add(func_name)
                if hook.get('override', False):
                    if func_name in override_hooks:
                        existing_feature, existing_callback = override_hooks[func_name]
                        print(f"ERROR: Multiple overrides found for '{func_name}':", file=sys.stderr)
                        print(f"  - {existing_feature}::{existing_callback} (from {hooks_path})", file=sys.stderr)
                        print(f"  - {feature_name}::{hook['callback']} (from {hooks_path})", file=sys.stderr)
                        print(f"Only one override is allowed per function across ALL features.", file=sys.stderr)
                        sys.exit(1)
                    override_hooks[func_name] = (feature_name, hook['callback'])
        except Exception as e:
            print(f"Error processing {hooks_path}: {e}", file=sys.stderr)
    
    features_dir = os.path.join(src_dir, 'features')
    core_dir = os.path.join(src_dir, 'core')
    
    for root, dirs, files in os.walk(src_dir):
        rel_path = os.path.relpath(root, src_dir)
        path_parts = rel_path.split(os.sep)
        
        if path_parts[0] == 'core':
            pass
        elif path_parts[0] == 'features' and len(path_parts) > 1:
            feature_name = path_parts[1]
            if feature_name not in enabled_features:
                dirs[:] = []
                continue
        else:
            continue
        
        for file in files:
            if file.endswith(('.cpp', '.c', '.h', '.hpp')):
                file_path = os.path.join(root, file)
                try:
                    with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                        content = f.read()
                        import re
                        matches = re.findall(r'namespace\s+detour_(\w+)\s*\{', content)
                        for match in matches:
                            used_detours.add(match)
                        for func_name in all_detours.keys():
                            if re.search(r'\bo' + func_name + r'\b', content):
                                used_detours.add(func_name)
                except Exception as e:
                    print(f"Warning: Could not scan {file_path}: {e}", file=sys.stderr)
    
    return used_detours, override_hooks

def generate_detours_header(detours, header_path, cpp_path, override_hooks, enabled_features):
    with open(header_path, 'w') as f:
        f.write('#pragma once\n\n')
        f.write('#include "detours.h"\n')
        f.write('#include "typed_shared_hook_manager.h"\n')
        f.write('#include "sof_compat.h"\n\n')
        f.write('// Auto-generated detour definitions\n')
        f.write('// DO NOT EDIT - Generated by tools/generate_hooks.py\n\n')
        
        for func_name, func in detours.items():
            namespace = f'detour_{func_name}'
            return_type = func['return_type']
            conv = func['calling_convention']
            params = func.get('params', [])
            module = func['module']
            identifier = func['identifier']
            detour_len = func.get('detour_len', 0)
            
            params_decl = ', '.join([f"{p['type']} {p['name']}" for p in params])
            params_call = ', '.join([p['name'] for p in params])
            
            f.write(f'namespace {namespace} {{\n')
            f.write(f'    using t{func_name} = {return_type}({conv}*)({params_decl});\n')
            f.write(f'    extern t{func_name} o{func_name};\n')
            
            if params:
                args_types = ', '.join([p['type'] for p in params])
                f.write(f'    using ManagerType = TypedSharedHookManager<{return_type}, {args_types}>;\n')
            else:
                f.write(f'    using ManagerType = TypedSharedHookManager<{return_type}>;\n')
            
            f.write(f'    ManagerType& GetManager();\n')
            f.write(f'    \n')
            
            if return_type == 'void':
                f.write(f'    void {conv} hk{func_name}({params_decl});\n')
            else:
                f.write(f'    {return_type} {conv} hk{func_name}({params_decl});\n')
            f.write(f'}}\n\n')
            
            f.write(f'namespace {{\n')
            f.write(f'    struct AutoDetour_{func_name} {{\n')
            f.write(f'        AutoDetour_{func_name}() {{\n')
            f.write(f'            using namespace {namespace};\n')
            f.write(f'            if (!GetDetourSystem().IsDetourRegistered("{func_name}")) {{\n')
            
            if identifier.startswith('0x') or identifier.startswith('0X'):
                addr_expr = f'reinterpret_cast<void*>({identifier})'
                f.write(f'                GetDetourSystem().RegisterDetour(\n')
                f.write(f'                    {addr_expr},\n')
            else:
                module_map = {
                    'RefDll': 'ref.dll',
                    'GameDll': 'game.dll',
                    'PlayerDll': 'player.dll',
                    'SofExe': None,
                    'Unknown': 'user32.dll'  # Default to user32.dll for Unknown module (Windows API functions)
                }
                dll_name = module_map.get(module, 'ref.dll')
                if dll_name:
                    f.write(f'                void* addr_{func_name} = nullptr;\n')
                    f.write(f'                {{\n')
                    f.write(f'                    HMODULE hMod = GetModuleHandleA("{dll_name}");\n')
                    f.write(f'                    if (hMod) addr_{func_name} = reinterpret_cast<void*>(GetProcAddress(hMod, "{identifier}"));\n')
                    f.write(f'                }}\n')
                    f.write(f'                GetDetourSystem().RegisterDetour(\n')
                    f.write(f'                    addr_{func_name},\n')
                else:
                    addr_expr = f'reinterpret_cast<void*>(GetProcAddress(GetModuleHandleA(NULL), "{identifier}"))'
                    f.write(f'                GetDetourSystem().RegisterDetour(\n')
                    f.write(f'                    {addr_expr},\n')
            f.write(f'                    reinterpret_cast<void*>({namespace}::hk{func_name}),\n')
            f.write(f'                    reinterpret_cast<void**>(&{namespace}::o{func_name}),\n')
            f.write(f'                    "{func_name}",\n')
            f.write(f'                    DetourModule::{module},\n')
            f.write(f'                    static_cast<size_t>({detour_len}));\n')
            f.write(f'            }}\n')
            f.write(f'        }}\n')
            f.write(f'    }};\n')
            f.write(f'    static AutoDetour_{func_name} g_AutoDetour_{func_name};\n')
            f.write(f'}}\n\n')
    
    with open(cpp_path, 'w') as f:
        f.write('// Auto-generated detour implementations\n')
        f.write('// DO NOT EDIT - Generated by tools/generate_hooks.py\n\n')
        f.write('#include "generated_detours.h"\n')
        f.write('#include "generated_registrations.h"\n')
        f.write('#include "util.h"\n\n')
        
        # Define the original function pointers
        for func_name, func in detours.items():
            namespace = f'detour_{func_name}'
            return_type = func['return_type']
            conv = func['calling_convention']
            params = func.get('params', [])
            params_decl = ', '.join([f"{p['type']} {p['name']}" for p in params])
            f.write(f'namespace {namespace} {{\n')
            f.write(f'    t{func_name} o{func_name} = nullptr;\n')
            f.write(f'}}\n\n')
        
        for func_name, func in detours.items():
            namespace = f'detour_{func_name}'
            return_type = func['return_type']
            params = func.get('params', [])
            
            if params:
                args_types = ', '.join([p['type'] for p in params])
                f.write(f'namespace {namespace} {{\n')
                f.write(f'    ManagerType& GetManager() {{\n')
                f.write(f'        static ManagerType* instance = nullptr;\n')
                f.write(f'        if (!instance) {{\n')
                f.write(f'            static char storage[sizeof(ManagerType)];\n')
                f.write(f'            instance = new(storage) ManagerType();\n')
                f.write(f'        }}\n')
                f.write(f'        return *instance;\n')
                f.write(f'    }}\n')
                f.write(f'}}\n\n')
            else:
                f.write(f'namespace {namespace} {{\n')
                f.write(f'    ManagerType& GetManager() {{\n')
                f.write(f'        static ManagerType* instance = nullptr;\n')
                f.write(f'        if (!instance) {{\n')
                f.write(f'            static char storage[sizeof(ManagerType)];\n')
                f.write(f'            instance = new(storage) ManagerType();\n')
                f.write(f'        }}\n')
                f.write(f'        return *instance;\n')
                f.write(f'    }}\n')
                f.write(f'}}\n\n')
        
        # First pass: Generate all normal hooks (non-override)
        for func_name, func in detours.items():
            if func_name in override_hooks:
                continue
            
            namespace = f'detour_{func_name}'
            return_type = func['return_type']
            conv = func['calling_convention']
            params = func.get('params', [])
            
            params_decl = ', '.join([f"{p['type']} {p['name']}" for p in params])
            params_call = ', '.join([p['name'] for p in params])
            
            f.write(f'namespace {namespace} {{\n')
            if return_type == 'void':
                f.write(f'    void {conv} hk{func_name}({params_decl}) {{\n')
                f.write(f'        ManagerType& mgr = GetManager();\n')
                if params:
                    param_refs = ', '.join([f'{p["name"]}' for p in params])
                    f.write(f'        if (mgr.GetPreCallbackCount() > 0) mgr.DispatchPre({param_refs});\n')
                else:
                    f.write(f'        if (mgr.GetPreCallbackCount() > 0) mgr.DispatchPre();\n')
                f.write(f'        if (o{func_name}) {{\n')
                f.write(f'            o{func_name}({params_call});\n')
                f.write(f'        }}\n')
                if params:
                    param_refs = ', '.join([f'{p["name"]}' for p in params])
                    f.write(f'        if (mgr.GetPostCallbackCount() > 0) mgr.DispatchPost({param_refs});\n')
                else:
                    f.write(f'        if (mgr.GetPostCallbackCount() > 0) mgr.DispatchPost();\n')
                f.write(f'    }}\n')
            else:
                f.write(f'    {return_type} {conv} hk{func_name}({params_decl}) {{\n')
                f.write(f'        ManagerType& mgr = GetManager();\n')
                if params:
                    param_refs = ', '.join([f'{p["name"]}' for p in params])
                    f.write(f'        if (mgr.GetPreCallbackCount() > 0) mgr.DispatchPre({param_refs});\n')
                else:
                    f.write(f'        if (mgr.GetPreCallbackCount() > 0) mgr.DispatchPre();\n')
                f.write(f'        {return_type} result{{}};\n')
                f.write(f'        if (o{func_name}) {{\n')
                f.write(f'            result = o{func_name}({params_call});\n')
                f.write(f'        }}\n')
                if params:
                    param_refs = ', '.join([f'{p["name"]}' for p in params])
                    f.write(f'        if (mgr.GetPostCallbackCount() > 0) result = mgr.DispatchPost(result, {param_refs});\n')
                else:
                    f.write(f'        if (mgr.GetPostCallbackCount() > 0) result = mgr.DispatchPost(result);\n')
                f.write(f'        return result;\n')
                f.write(f'    }}\n')
            f.write(f'}}\n\n')
        
        # Second pass: Generate all override stubs together
        if override_hooks:
            f.write('// Override hooks (hooks.json override: true)\n')
            for func_name in sorted(override_hooks.keys()):
                if func_name not in detours:
                    continue
                func = detours[func_name]
                namespace = f'detour_{func_name}'
                return_type = func['return_type']
                conv = func['calling_convention']
                params = func.get('params', [])
                
                params_decl = ', '.join([f"{p['type']} {p['name']}" for p in params])
                params_call = ', '.join([p['name'] for p in params])
                
                feature_name, callback_name = override_hooks[func_name]
                if feature_name not in enabled_features and feature_name != 'core':
                    continue
                f.write(f'namespace {namespace} {{\n')
                if return_type == 'void':
                    if params:
                        f.write(f'    void {conv} hk{func_name}({params_decl}) {{\n')
                        f.write(f'        ::{callback_name}({params_call}, o{func_name});\n')
                        f.write(f'    }}\n')
                    else:
                        f.write(f'    void {conv} hk{func_name}() {{\n')
                        f.write(f'        ::{callback_name}(o{func_name});\n')
                        f.write(f'    }}\n')
                else:
                    if params:
                        f.write(f'    {return_type} {conv} hk{func_name}({params_decl}) {{\n')
                        f.write(f'        return ::{callback_name}({params_call}, o{func_name});\n')
                        f.write(f'    }}\n')
                    else:
                        f.write(f'    {return_type} {conv} hk{func_name}() {{\n')
                        f.write(f'        return ::{callback_name}(o{func_name});\n')
                        f.write(f'    }}\n')
                f.write(f'}}\n\n')

def generate_registrations_header(detours, feature_hooks, feature_callbacks, output_path, override_hooks):
    callback_decls = {}
    
    hook_params = {
        'RefDllLoaded': 'char const* name',
        'GameDllLoaded': 'void* imports',
    }
    
    for feature_name, callbacks_path in feature_callbacks:
        try:
            callbacks = parse_callbacks_json(callbacks_path)
            for callback in callbacks:
                callback_name = callback['callback']
                hook_name = callback['hook']
                if hook_name in hook_params:
                    callback_decls[callback_name] = f'void {callback_name}({hook_params[hook_name]});'
                else:
                    callback_decls[callback_name] = f'void {callback_name}();'
        except Exception as e:
            print(f"Error processing {callbacks_path}: {e}", file=sys.stderr)
    
    for feature_name, hooks_path in feature_hooks:
        try:
            hooks = parse_hooks_json(hooks_path)
            for hook in hooks:
                func_name = hook['function']
                callback_name = hook['callback']
                phase = hook.get('phase', 'Post')
                is_override = hook.get('override', False)
                
                if func_name not in detours:
                    continue
                
                func = detours[func_name]
                return_type = func['return_type']
                params = func.get('params', [])
                
                # For override hooks, generate callback signature that includes original function pointer
                if is_override:
                    if params:
                        params_decl = ', '.join([f"{p['type']} {p['name']}" for p in params])
                        if return_type == 'void':
                            callback_decls[callback_name] = f'void {callback_name}({params_decl}, detour_{func_name}::t{func_name} original);'
                        else:
                            callback_decls[callback_name] = f'{return_type} {callback_name}({params_decl}, detour_{func_name}::t{func_name} original);'
                    else:
                        if return_type == 'void':
                            callback_decls[callback_name] = f'void {callback_name}(detour_{func_name}::t{func_name} original);'
                        else:
                            callback_decls[callback_name] = f'{return_type} {callback_name}(detour_{func_name}::t{func_name} original);'
                    continue
                
                # Normal hook registration (non-override)
                if phase == 'Pre':
                    if params:
                        param_decl = ', '.join([f'{p["type"]}& {p["name"]}' for p in params])
                        callback_decls[callback_name] = f'void {callback_name}({param_decl});'
                    else:
                        callback_decls[callback_name] = f'void {callback_name}();'
                else:
                    if return_type == 'void':
                        if params:
                            param_decl = ', '.join([f'{p["type"]} {p["name"]}' for p in params])
                            callback_decls[callback_name] = f'void {callback_name}({param_decl});'
                        else:
                            callback_decls[callback_name] = f'void {callback_name}();'
                    else:
                        if params:
                            param_decl = ', '.join([f'{p["type"]} {p["name"]}' for p in params])
                            callback_decls[callback_name] = f'{return_type} {callback_name}({return_type} result, {param_decl});'
                        else:
                            callback_decls[callback_name] = f'{return_type} {callback_name}({return_type} result);'
        except Exception as e:
            print(f"Error processing {hooks_path}: {e}", file=sys.stderr)
    
    with open(output_path, 'w') as f:
        f.write('#pragma once\n\n')
        f.write('// Auto-generated hook registrations\n')
        f.write('// DO NOT EDIT - Generated by tools/generate_hooks.py\n\n')
        f.write('#include "sof_compat.h"\n')
        f.write('#include "shared_hook_manager.h"\n')
        f.write('#include "util.h"\n\n')
        
        for callback_name in sorted(callback_decls.keys()):
            f.write(f'extern {callback_decls[callback_name]}\n')
        
        f.write('\ninline void RegisterAllFeatureHooks() {\n')
        
        hook_params = {
            'RefDllLoaded': 'char const* name',
            'GameDllLoaded': 'void* imports',
        }
        
        for feature_name, callbacks_path in feature_callbacks:
            try:
                callbacks = parse_callbacks_json(callbacks_path)
                for callback in callbacks:
                    hook_name = callback['hook']
                    callback_name = callback['callback']
                    priority = callback.get('priority', 0)
                    phase = callback.get('phase', 'Post')
                    
                    if hook_name in hook_params:
                        param_decl = hook_params[hook_name]
                        param_name = param_decl.split()[-1]
                        param_type = param_decl.rsplit(' ', 1)[0]
                        f.write(f'    SharedHookManager::Instance().RegisterCallback<{param_type}>(\n')
                        f.write(f'        "{hook_name}", "{feature_name}", "{callback_name}",\n')
                        f.write(f'        std::function<void({param_type})>([]({param_decl}) {{ {callback_name}({param_name}); }}), {priority}, SharedHookPhase::{phase});\n')
                    else:
                        f.write(f'    SharedHookManager::Instance().RegisterCallback(\n')
                        f.write(f'        "{hook_name}", "{feature_name}", "{callback_name}",\n')
                        f.write(f'        []() {{ {callback_name}(); }}, {priority}, SharedHookPhase::{phase});\n')
            except Exception as e:
                print(f"Error processing {callbacks_path}: {e}", file=sys.stderr)
        
        for feature_name, hooks_path in feature_hooks:
            try:
                hooks = parse_hooks_json(hooks_path)
                for hook in hooks:
                    func_name = hook['function']
                    callback_name = hook['callback']
                    phase = hook.get('phase', 'Post')
                    priority = hook.get('priority', 0)
                    is_override = hook.get('override', False)
                    
                    if func_name not in detours:
                        if not is_override:
                            print(f"Warning: Function '{func_name}' not found in detours.yaml (feature: {feature_name})", file=sys.stderr)
                        continue
                    
                    # Skip registration for override hooks - they handle everything themselves
                    if is_override:
                        continue
                    
                    namespace = f'detour_{func_name}'
                    func = detours[func_name]
                    params = func.get('params', [])
                    
                    if phase == 'Pre':
                        if params:
                            param_refs = ', '.join([f'{p["type"]}& {p["name"]}' for p in params])
                            f.write(f'    {namespace}::GetManager().RegisterPreCallback(\n')
                            f.write(f'        "{feature_name}", "{callback_name}",\n')
                            f.write(f'        []({param_refs}) {{ {callback_name}({", ".join([p["name"] for p in params])}); }},\n')
                            f.write(f'        {priority});\n')
                        else:
                            f.write(f'    {namespace}::GetManager().RegisterPreCallback(\n')
                            f.write(f'        "{feature_name}", "{callback_name}",\n')
                            f.write(f'        []() {{ {callback_name}(); }},\n')
                            f.write(f'        {priority});\n')
                    else:
                        return_type = func['return_type']
                        if return_type == 'void':
                            if params:
                                param_refs = ', '.join([f'{p["type"]} {p["name"]}' for p in params])
                                f.write(f'    {{\n')
                                f.write(f'        auto& mgr = {namespace}::GetManager();\n')
                                f.write(f'        PrintOut(PRINT_LOG, "[RegisterAllFeatureHooks] {func_name} manager at 0x%p\\n", &mgr);\n')
                                f.write(f'        mgr.RegisterPostCallback(\n')
                                f.write(f'            "{feature_name}", "{callback_name}",\n')
                                f.write(f'            []({param_refs}) {{ {callback_name}({", ".join([p["name"] for p in params])}); }},\n')
                                f.write(f'            {priority});\n')
                                f.write(f'        PrintOut(PRINT_LOG, "[RegisterAllFeatureHooks] {func_name} post callbacks: %zu\\n", mgr.GetPostCallbackCount());\n')
                                f.write(f'    }}\n')
                            else:
                                f.write(f'    {{\n')
                                f.write(f'        auto& mgr = {namespace}::GetManager();\n')
                                f.write(f'        PrintOut(PRINT_LOG, "[RegisterAllFeatureHooks] {func_name} manager at 0x%p\\n", &mgr);\n')
                                f.write(f'        mgr.RegisterPostCallback(\n')
                                f.write(f'            "{feature_name}", "{callback_name}",\n')
                                f.write(f'            []() {{ {callback_name}(); }},\n')
                                f.write(f'            {priority});\n')
                                f.write(f'        PrintOut(PRINT_LOG, "[RegisterAllFeatureHooks] {func_name} post callbacks: %zu\\n", mgr.GetPostCallbackCount());\n')
                                f.write(f'    }}\n')
                        else:
                            if params:
                                param_refs = ', '.join([f'{p["type"]} {p["name"]}' for p in params])
                                f.write(f'    {namespace}::GetManager().RegisterPostCallback(\n')
                                f.write(f'        "{feature_name}", "{callback_name}",\n')
                                f.write(f'        []({return_type} result, {param_refs}) {{ return {callback_name}(result, {", ".join([p["name"] for p in params])}); }},\n')
                                f.write(f'        {priority});\n')
                            else:
                                f.write(f'    {namespace}::GetManager().RegisterPostCallback(\n')
                                f.write(f'        "{feature_name}", "{callback_name}",\n')
                                f.write(f'        []({return_type} result) {{ return {callback_name}(result); }},\n')
                                f.write(f'        {priority});\n')
            except Exception as e:
                print(f"Error processing {hooks_path}: {e}", file=sys.stderr)
        
        f.write('}\n')

def main():
    script_dir = Path(__file__).parent
    project_root = script_dir.parent
    
    yaml_path = project_root / 'detours.yaml'
    features_txt_path = project_root / 'features' / 'FEATURES.txt'
    features_dir = project_root / 'src' / 'features'
    core_dir = project_root / 'src' / 'core'
    src_dir = project_root / 'src'
    build_dir = project_root / 'build'
    build_dir.mkdir(exist_ok=True)
    
    detours_output = build_dir / 'generated_detours.h'
    detours_cpp_output = build_dir / 'generated_detours.cpp'
    registrations_output = build_dir / 'generated_registrations.h'
    
    enabled_features = parse_features_txt(features_txt_path)
    
    all_detours = parse_detours_yaml(yaml_path)
    all_feature_hooks = find_hooks_json_files(features_dir, core_dir)
    all_feature_callbacks = find_callbacks_json_files(features_dir, core_dir)
    
    # Include base features when their dependent features are enabled
    # e.g., if scaled_con is enabled, also include scaled_ui_base
    base_features_to_include = set()
    for feature_name in enabled_features:
        if feature_name.startswith('scaled_') and feature_name != 'scaled_ui_base':
            base_features_to_include.add('scaled_ui_base')
    
    effective_enabled_features = enabled_features | base_features_to_include
    
    feature_hooks = [(name, path) for name, path in all_feature_hooks if name == 'core' or name in effective_enabled_features]
    feature_callbacks = [(name, path) for name, path in all_feature_callbacks if name == 'core' or name in effective_enabled_features]
    
    used_detours, override_hooks = find_used_detours(feature_hooks, src_dir, all_detours, enabled_features)
    
    detours = {name: func for name, func in all_detours.items() if name in used_detours}
    
    unused_count = len(all_detours) - len(detours)
    if unused_count > 0:
        print(f"Info: Skipping {unused_count} unused detour(s) (only generating {len(detours)} used detour(s))", file=sys.stderr)
    
    if override_hooks:
        print(f"Info: Found {len(override_hooks)} override hook(s) from hooks.json: {', '.join([f'{name} ({feature}::{callback})' for name, (feature, callback) in sorted(override_hooks.items())])}", file=sys.stderr)
    
    generate_detours_header(detours, detours_output, detours_cpp_output, override_hooks, effective_enabled_features)
    generate_registrations_header(detours, feature_hooks, feature_callbacks, registrations_output, override_hooks)
    
    print(f"Generated {detours_output}")
    print(f"Generated {detours_cpp_output}")
    print(f"Generated {registrations_output}")

if __name__ == '__main__':
    main()

