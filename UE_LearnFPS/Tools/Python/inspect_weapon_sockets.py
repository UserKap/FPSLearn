"""
只读：打印 BP_MCX300 的武器骨骼、部件和枪口、瞄准插槽在武器网格组件空间里的位置和轴向。
结果写到 Saved/WeaponSockets_Result.txt。
"""

import os
import traceback

import unreal

BLUEPRINT_PATH = "/Game/CustomEditorBP/Weapon/BP_MCX300"
RESULT_FILE = os.path.join(unreal.Paths.project_saved_dir(), "WeaponSockets_Result.txt")


def fmt_vec(v):
    return "({:.2f}, {:.2f}, {:.2f})".format(v.x, v.y, v.z)


def describe(label, transform, lines):
    rot = transform.rotation
    lines.append("{}: loc={} rot={} X={} Y={} Z={}".format(
        label,
        fmt_vec(transform.translation),
        transform.rotation.rotator(),
        fmt_vec(rot.get_axis_x()),
        fmt_vec(rot.get_axis_y()),
        fmt_vec(rot.get_axis_z())))


def main():
    lines = []
    try:
        bp_class = unreal.EditorAssetLibrary.load_blueprint_class(BLUEPRINT_PATH)
        cdo = unreal.get_default_object(bp_class)
        weapon_mesh = cdo.get_editor_property("weapon_mesh")
        skeletal_mesh = weapon_mesh.get_editor_property("skeletal_mesh_asset")
        lines.append("WeaponMesh asset: " + skeletal_mesh.get_path_name())
        lines.append("WeaponMesh relative: " + str(weapon_mesh.get_editor_property("relative_location")) + " " + str(weapon_mesh.get_editor_property("relative_rotation")))
        lines.append("AimSocket name: " + str(cdo.get_editor_property("aim_socket")))
        lines.append("FireSoundPart: " + str(cdo.get_editor_property("fire_sound_part")) + " socket: " + str(cdo.get_editor_property("fire_sound_socket")))

        skeleton = skeletal_mesh.get_editor_property("skeleton")
        pose = unreal.AnimPoseExtensions.get_reference_pose(skeleton)
        bone_names = unreal.AnimPoseExtensions.get_bone_names(pose)
        lines.append("Bones: " + ", ".join(str(b) for b in bone_names))
        bone_world = {}
        for bone in bone_names:
            t = unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD)
            bone_world[str(bone)] = t
            describe("Bone " + str(bone), t, lines)

        for bone, t in bone_world.items():
            lines.append("BoneScale {} {}".format(bone, fmt_vec(t.scale3d)))

        for index in range(skeletal_mesh.num_sockets()):
            socket = skeletal_mesh.get_socket_by_index(index)
            bone = str(socket.get_editor_property("bone_name"))
            local = socket.get_socket_local_transform()
            if bone in bone_world:
                composed = unreal.MathLibrary.compose_transforms(local, bone_world[bone])
                describe("MeshSocket {} (bone {})".format(socket.get_editor_property("socket_name"), bone), composed, lines)

        for part in cdo.get_editor_property("weapon_parts"):
            mesh = part.get_editor_property("mesh")
            attach = str(part.get_editor_property("attach_socket"))
            lines.append("Part {} socket={} mesh={}".format(part.get_editor_property("part_name"), attach, mesh.get_path_name() if mesh else None))
            if mesh:
                socket = mesh.find_socket("Muzzle")
                if socket:
                    lines.append("  StaticSocket Muzzle loc={} rot={}".format(
                        socket.get_editor_property("relative_location"),
                        socket.get_editor_property("relative_rotation")))
                    if attach in bone_world:
                        part_socket = unreal.Transform(socket.get_editor_property("relative_location"), socket.get_editor_property("relative_rotation"), socket.get_editor_property("relative_scale"))
                        mesh_socket = skeletal_mesh.find_socket(attach)
                        attach_world = bone_world[attach]
                        if mesh_socket:
                            attach_world = unreal.MathLibrary.compose_transforms(mesh_socket.get_socket_local_transform(), attach_world)
                        describe("  Part {} Muzzle in weapon space".format(attach), unreal.MathLibrary.compose_transforms(part_socket, attach_world), lines)
                bounds = mesh.get_bounding_box()
                lines.append("  Bounds min={} max={}".format(fmt_vec(bounds.min), fmt_vec(bounds.max)))
    except Exception:
        lines.append("FAILED " + traceback.format_exc())

    with open(RESULT_FILE, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))


main()
