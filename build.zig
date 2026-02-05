const std = @import("std");

// todo: if x64 is compiled compile hook x86 ver

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{ .default_target = .{ .os_tag = .windows } });
    const optimize = b.standardOptimizeOption(.{});

    const nuklear = b.dependency("nuklear", .{
        .target = target,
        .optimize = optimize,
    });

    const stb = b.dependency("stb", .{
        .target = target,
        .optimize = optimize,
    });

    const flags = &[_][]const u8 {
        "-Wall",
        "-Wextra",
        "-Wpedantic",
        "-Werror",
        "-std=c23",
    };

    const hook_mod = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });

    const hook = b.addExecutable(.{
        .name = "hook",
        .root_module = hook_mod,
    });

    hook.linkLibC();
    hook.linkSystemLibrary("user32");
    hook.linkSystemLibrary("dwmapi");

    hook.addIncludePath(stb.path(""));

    hook.subsystem = .Windows;
    
    hook.addCSourceFile(.{
        .file = b.path("src/hook.c"),
        .flags = flags,
    });

    b.installArtifact(hook);
    
    const overlap_mod = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });

    const overlap = b.addExecutable(.{
        .name = "overlap",
        .root_module = overlap_mod,
    });

    overlap.linkLibC();
    overlap.linkSystemLibrary("user32");
    overlap.linkSystemLibrary("dwmapi");
    overlap.linkSystemLibrary("d3d9");

    overlap.addIncludePath(nuklear.path(""));
    overlap.addIncludePath(nuklear.path("demo/d3d9"));
    overlap.addIncludePath(stb.path(""));

    overlap.subsystem = .Windows;

    overlap.addCSourceFile(.{
        .file = b.path("src/app.c"),
        .flags = flags,
    });

    b.installArtifact(overlap);
}
