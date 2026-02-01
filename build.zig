const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{ .default_target = .{ .os_tag = .windows } });
    const optimize = b.standardOptimizeOption(.{});

    const nuklear = b.dependency("nuklear", .{
        .target = target,
        .optimize = optimize,
    });

    const exe_mod = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });

    const exe = b.addExecutable(.{
        .name = "overlap",
        .root_module = exe_mod,
    });

    exe.linkLibC();
    exe.linkSystemLibrary("user32");
    exe.linkSystemLibrary("dwmapi");
    exe.linkSystemLibrary("d3d9");

    const flags = &[_][]const u8 {
        "-Wall",
        "-Wextra",
        "-Wpedantic",
        "-Werror",
        "-std=c23",
    };

    exe.addIncludePath(nuklear.path(""));
    exe.addIncludePath(nuklear.path("demo/d3d9"));
    
    exe.addCSourceFile(.{
        .file = b.path("main.c"),
        .flags = flags,
    });
    
    exe.subsystem = .Windows;

    b.installArtifact(exe);
}
