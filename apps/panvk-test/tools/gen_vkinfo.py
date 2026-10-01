#!/usr/bin/env python3
import os
import sys
import re
import argparse
import xml.etree.ElementTree as ET

def find_repo_root():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    # repo root is 3 levels up from apps/panvk-test/tools
    candidate = os.path.abspath(os.path.join(script_dir, "..", "..", ".."))
    if os.path.exists(os.path.join(candidate, "tests", "dxvk", "vulkan")):
        return candidate
    cwd = os.getcwd()
    if os.path.exists(os.path.join(cwd, "tests", "dxvk", "vulkan")):
        return cwd
    return candidate

def main():
    repo_root = find_repo_root()
    parser = argparse.ArgumentParser(description="Generate vkinfo_gen.h from vk.xml and vulkan_core.h")
    parser.add_argument("--registry", default=os.path.join(repo_root, "work", "mesa-dxint", "src", "vulkan", "registry", "vk.xml"),
                        help="Path to vk.xml")
    parser.add_argument("--header", default=os.path.join(repo_root, "work", "mesa-dxint", "include", "vulkan", "vulkan_core.h"),
                        help="Path to vulkan_core.h")
    parser.add_argument("--output", default=os.path.join(repo_root, "apps", "panvk-test", "app", "src", "main", "cpp", "vkinfo_gen.h"),
                        help="Path to output vkinfo_gen.h")
    args = parser.parse_args()

    if not os.path.exists(args.registry):
        sys.exit(f"Registry file not found: {args.registry}")
    if not os.path.exists(args.header):
        sys.exit(f"Header file not found: {args.header}")

    with open(args.header, "r", encoding="utf-8") as f:
        header_text = f.read()

    tree = ET.parse(args.registry)
    root = tree.getroot()

    # 1. Parse sTypes from vulkan_core.h
    stypes_from_header = {}
    for m in re.finditer(r'(VK_STRUCTURE_TYPE_[A-Za-z0-9_]+)\s*=\s*([0-9]+)', header_text):
        stypes_from_header[m.group(1)] = int(m.group(2))
    for m in re.finditer(r'(VK_STRUCTURE_TYPE_[A-Za-z0-9_]+)\s*=\s*(VK_STRUCTURE_TYPE_[A-Za-z0-9_]+)', header_text):
        if m.group(2) in stypes_from_header:
            stypes_from_header[m.group(1)] = stypes_from_header[m.group(2)]

    # Also parse sType mappings from vk.xml struct definitions
    stype_for_struct = {}
    for s in root.findall('./types/type[@category="struct"]'):
        s_name = s.get('name')
        for m in s.findall('./member'):
            mn = m.find('name')
            if mn is not None and mn.text == 'sType':
                stype_for_struct[s_name] = m.get('values')

    # Type aliases
    aliases = {}
    for t in root.findall('./types/type'):
        name = t.get('name')
        alias = t.get('alias')
        if name and alias:
            aliases[name] = alias

    # Map extensions to types
    ext_for_type = {}
    for ext in root.findall('./extensions/extension'):
        ext_name = ext.get('name')
        # Skip beta / provisional / platform-specific extensions
        if ext.get('provisional') == 'true' or ext.get('platform') or 'beta' in ext_name.lower():
            continue
        for req in ext.findall('./require'):
            for t in req.findall('./type'):
                tname = t.get('name')
                if tname:
                    ext_for_type[tname] = ext_name
                    if tname in aliases:
                        ext_for_type[aliases[tname]] = ext_name

    # Core Vulkan 1.1-1.4 feature structs
    core_version_map = {
        'VkPhysicalDeviceVulkan11Features': 'VK_API_VERSION_1_1',
        'VkPhysicalDeviceVulkan12Features': 'VK_API_VERSION_1_2',
        'VkPhysicalDeviceVulkan13Features': 'VK_API_VERSION_1_3',
        'VkPhysicalDeviceVulkan14Features': 'VK_API_VERSION_1_4',
    }

    # Find feature structs
    feature_structs = []
    for struct in root.findall('./types/type[@category="struct"]'):
        name = struct.get('name')
        if not name or name not in header_text:
            continue
        structextends = struct.get('structextends', '')
        if 'VkPhysicalDeviceFeatures2' not in [s.strip() for s in structextends.split(',')]:
            continue

        members = struct.findall('./member')
        if len(members) <= 2:
            continue
        m0_n = members[0].find('name')
        m0_t = members[0].find('type')
        m1_n = members[1].find('name')
        if m0_n is None or m0_n.text != 'sType' or m0_t is None or m0_t.text != 'VkStructureType' or m1_n is None or m1_n.text != 'pNext':
            continue

        # Check all members after pNext are VkBool32
        bool_members = []
        valid_bools = True
        for m in members[2:]:
            mt = m.find('type')
            mn = m.find('name')
            if mt is None or mt.text != 'VkBool32' or mn is None or not mn.text:
                valid_bools = False
                break
            bool_members.append(mn.text)
        if not valid_bools:
            continue

        stype_enum = stype_for_struct.get(name)
        if not stype_enum or stype_enum not in stypes_from_header:
            continue
        stype_val = stypes_from_header[stype_enum]

        # Determine extension / core version
        ext_name = ext_for_type.get(name)
        core_ver = core_version_map.get(name)
        if not ext_name and not core_ver:
            # Skip if not an extension feature struct and not core 1.1-1.4
            continue

        feature_structs.append({
            'name': name,
            'stype_enum': stype_enum,
            'stype_val': stype_val,
            'ext_name': ext_name,
            'core_ver': core_ver,
            'members': bool_members
        })

    # Sort feature structs: core first (1.1, 1.2, 1.3, 1.4), then extensions alphabetically
    def sort_key(s):
        if s['core_ver']:
            return (0, s['core_ver'], s['name'])
        return (1, s['ext_name'] or '', s['name'])
    feature_structs.sort(key=sort_key)

    # 2. Parse VkPhysicalDeviceLimits and VkPhysicalDeviceSparseProperties
    def parse_struct_members(struct_name):
        node = root.find(f'./types/type[@name="{struct_name}"]')
        res = []
        for m in node.findall('./member'):
            mt = m.find('type').text
            mn = m.find('name').text
            full = ''.join(m.itertext())
            arr_dim = None
            marr = re.search(r'\[([0-9]+)\]', full)
            if marr:
                arr_dim = int(marr.group(1))
            res.append({'type': mt, 'name': mn, 'array': arr_dim})
        return res

    limits_members = parse_struct_members('VkPhysicalDeviceLimits')
    sparse_members = parse_struct_members('VkPhysicalDeviceSparseProperties')

    # Core 1.0 VkPhysicalDeviceFeatures
    core_features_node = root.find('./types/type[@name="VkPhysicalDeviceFeatures"]')
    core_features_members = [m.find('name').text for m in core_features_node.findall('./member') if m.find('name') is not None]

    # 3. Parse VkFormat enums from vulkan_core.h
    formats = {}
    aliases_fmt = {}
    for m in re.finditer(r'(VK_FORMAT_[A-Za-z0-9_]+)\s*=\s*([0-9]+)', header_text):
        fn = m.group(1)
        if any(x in fn for x in ('MAX_ENUM', 'BEGIN_RANGE', 'END_RANGE', 'RANGE_SIZE')):
            continue
        formats[fn] = int(m.group(2))

    for m in re.finditer(r'(VK_FORMAT_[A-Za-z0-9_]+)\s*=\s*(VK_FORMAT_[A-Za-z0-9_]+)', header_text):
        fn = m.group(1)
        target = m.group(2)
        if any(x in fn for x in ('MAX_ENUM', 'BEGIN_RANGE', 'END_RANGE', 'RANGE_SIZE')):
            continue
        aliases_fmt[fn] = target

    for fn, target in aliases_fmt.items():
        if target in formats:
            formats[fn] = formats[target]

    # Deduplicate aliases by value: keep the most standard/canonical name
    by_val = {}
    for fn, val in formats.items():
        if val not in by_val:
            by_val[val] = fn
        else:
            curr = by_val[val]
            is_curr_ext = any(curr.endswith(x) for x in ('_KHR', '_EXT', '_NV', '_ARM'))
            is_fn_ext = any(fn.endswith(x) for x in ('_KHR', '_EXT', '_NV', '_ARM'))
            if is_curr_ext and not is_fn_ext:
                by_val[val] = fn
            elif not is_curr_ext and is_fn_ext:
                pass
            elif len(fn) < len(curr):
                by_val[val] = fn

    unique_formats = sorted(by_val.items(), key=lambda x: x[0])

    # 4. Generate C code
    os.makedirs(os.path.dirname(args.output), exist_ok=True)
    with open(args.output, "w", encoding="utf-8") as out:
        out.write("/* Auto-generated by gen_vkinfo.py -- DO NOT EDIT */\n")
        out.write("#ifndef VKINFO_GEN_H\n")
        out.write("#define VKINFO_GEN_H\n\n")
        out.write("#include <stdio.h>\n")
        out.write("#include <stdint.h>\n")
        out.write("#include <stdbool.h>\n")
        out.write("#include <inttypes.h>\n")
        out.write("#include <vulkan/vulkan.h>\n\n")

        # Feature structs member arrays and print functions
        out.write("/* ======================================================= */\n")
        out.write("/* Feature Structs Member Arrays and Print Functions       */\n")
        out.write("/* ======================================================= */\n\n")

        for s in feature_structs:
            s_name = s['name']
            m_list = s['members']
            out.write(f"static const char * const {s_name}_members[] = {{\n")
            for m in m_list:
                out.write(f'    "{m}",\n')
            out.write("};\n\n")

            out.write(f"static void print_members_{s_name}(FILE *out, const void *ptr) {{\n")
            out.write(f"    const {s_name} *s = (const {s_name} *)ptr;\n")
            for i, m in enumerate(m_list):
                comma = "," if i + 1 < len(m_list) else ""
                out.write(f'    fprintf(out, "\\"{m}\\":%s{comma}", s->{m} ? "true" : "false");\n')
            out.write("}\n\n")

        # Table of feature structs
        out.write("/* ======================================================= */\n")
        out.write("/* Table of Feature Structs                                */\n")
        out.write("/* ======================================================= */\n\n")
        out.write("typedef struct {\n")
        out.write("    const char *struct_name;\n")
        out.write("    VkStructureType sType;\n")
        out.write("    size_t size;\n")
        out.write("    const char *ext_name; /* NULL for core 1.1-1.4 */\n")
        out.write("    uint32_t core_version; /* e.g. VK_API_VERSION_1_1, or 0 for extensions */\n")
        out.write("    const char * const *member_names;\n")
        out.write("    size_t member_count;\n")
        out.write("    void (*print_members)(FILE *out, const void *ptr);\n")
        out.write("} VkFeatureStructDesc;\n\n")

        out.write("static const VkFeatureStructDesc vkinfo_feature_structs[] = {\n")
        for s in feature_structs:
            s_name = s['name']
            ext_val = f'"{s["ext_name"]}"' if s['ext_name'] else "NULL"
            core_val = s['core_ver'] if s['core_ver'] else "0"
            out.write("    {\n")
            out.write(f'        "{s_name}",\n')
            out.write(f'        (VkStructureType){s["stype_val"]}, /* {s["stype_enum"]} */\n')
            out.write(f'        sizeof({s_name}),\n')
            out.write(f'        {ext_val},\n')
            out.write(f'        {core_val},\n')
            out.write(f'        {s_name}_members,\n')
            out.write(f'        sizeof({s_name}_members) / sizeof({s_name}_members[0]),\n')
            out.write(f'        print_members_{s_name}\n')
            out.write("    },\n")
        out.write("};\n\n")
        out.write("static const size_t vkinfo_feature_struct_count = sizeof(vkinfo_feature_structs) / sizeof(vkinfo_feature_structs[0]);\n\n")

        # Core 1.0 VkPhysicalDeviceFeatures print function
        out.write("/* ======================================================= */\n")
        out.write("/* Core 1.0 VkPhysicalDeviceFeatures Print Function        */\n")
        out.write("/* ======================================================= */\n\n")
        out.write("static void print_VkPhysicalDeviceFeatures_members(FILE *out, const VkPhysicalDeviceFeatures *s) {\n")
        for i, m in enumerate(core_features_members):
            comma = "," if i + 1 < len(core_features_members) else ""
            out.write(f'    fprintf(out, "\\"{m}\\":%s{comma}", s->{m} ? "true" : "false");\n')
        out.write("}\n\n")

        # Limits print function
        out.write("/* ======================================================= */\n")
        out.write("/* VkPhysicalDeviceLimits and SparseProperties Functions  */\n")
        out.write("/* ======================================================= */\n\n")

        out.write("static void print_VkPhysicalDeviceLimits_json(FILE *out, const VkPhysicalDeviceLimits *s) {\n")
        for i, m in enumerate(limits_members):
            comma = "," if i + 1 < len(limits_members) else ""
            m_type = m['type']
            m_name = m['name']
            arr = m['array']
            if arr:
                if m_type == 'uint32_t':
                    elements = ", ".join([f"s->{m_name}[{idx}]" for idx in range(arr)])
                    format_spec = ",".join(["%u"] * arr)
                    out.write(f'    fprintf(out, "\\"{m_name}\\":[{format_spec}]{comma}", {elements});\n')
                elif m_type == 'float':
                    elements = ", ".join([f"(double)s->{m_name}[{idx}]" for idx in range(arr)])
                    format_spec = ",".join(["%.6g"] * arr)
                    out.write(f'    fprintf(out, "\\"{m_name}\\":[{format_spec}]{comma}", {elements});\n')
                else:
                    out.write(f'    /* unhandled array {m_type} {m_name}[{arr}] */\n')
            else:
                if m_type == 'uint32_t':
                    out.write(f'    fprintf(out, "\\"{m_name}\\":%u{comma}", s->{m_name});\n')
                elif m_type == 'int32_t':
                    out.write(f'    fprintf(out, "\\"{m_name}\\":%d{comma}", s->{m_name});\n')
                elif m_type == 'float':
                    out.write(f'    fprintf(out, "\\"{m_name}\\":%.6g{comma}", (double)s->{m_name});\n')
                elif m_type == 'size_t':
                    out.write(f'    fprintf(out, "\\"{m_name}\\":%zu{comma}", s->{m_name});\n')
                elif m_type == 'VkDeviceSize':
                    out.write(f'    fprintf(out, "\\"{m_name}\\":%" PRIu64 "{comma}", (uint64_t)s->{m_name});\n')
                elif m_type == 'VkBool32':
                    out.write(f'    fprintf(out, "\\"{m_name}\\":%s{comma}", s->{m_name} ? "true" : "false");\n')
                elif m_type == 'VkSampleCountFlags':
                    out.write(f'    fprintf(out, "\\"{m_name}\\":%u{comma}", (uint32_t)s->{m_name});\n')
                else:
                    out.write(f'    /* unhandled scalar {m_type} {m_name} */\n')
        out.write("}\n\n")

        # Sparse Properties print function
        out.write("static void print_VkPhysicalDeviceSparseProperties_json(FILE *out, const VkPhysicalDeviceSparseProperties *s) {\n")
        for i, m in enumerate(sparse_members):
            comma = "," if i + 1 < len(sparse_members) else ""
            m_name = m['name']
            out.write(f'    fprintf(out, "\\"{m_name}\\":%s{comma}", s->{m_name} ? "true" : "false");\n')
        out.write("}\n\n")

        # Combined function for limits and sparse properties
        out.write("static void print_limits_and_sparse_json(FILE *out, const VkPhysicalDeviceLimits *l, const VkPhysicalDeviceSparseProperties *sp) {\n")
        out.write('    fprintf(out, "\\"limits\\":{");\n')
        out.write('    print_VkPhysicalDeviceLimits_json(out, l);\n')
        out.write('    fprintf(out, "},\\"sparseProperties\\":{");\n')
        out.write('    print_VkPhysicalDeviceSparseProperties_json(out, sp);\n')
        out.write('    fprintf(out, "}");\n')
        out.write("}\n\n")

        # Formats Table
        out.write("/* ======================================================= */\n")
        out.write("/* Table of VkFormat Enums                                 */\n")
        out.write("/* ======================================================= */\n\n")
        out.write("typedef struct {\n")
        out.write("    const char *name;\n")
        out.write("    VkFormat format;\n")
        out.write("} VkFormatDesc;\n\n")

        out.write("static const VkFormatDesc vk_format_table[] = {\n")
        for val, name in unique_formats:
            out.write(f'    {{ "{name}", (VkFormat){val} }},\n')
        out.write("};\n\n")
        out.write("static const size_t vk_format_table_count = sizeof(vk_format_table) / sizeof(vk_format_table[0]);\n\n")

        out.write("#endif /* VKINFO_GEN_H */\n")

    print(f"Generated {args.output} successfully with {len(feature_structs)} feature structs and {len(unique_formats)} formats.")

if __name__ == "__main__":
    main()
