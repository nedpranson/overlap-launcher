const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{ .default_target = .{ .os_tag = .windows } });
    const optimize = b.standardOptimizeOption(.{});

    //const nuklear = b.dependency("nuklear", .{
        //.target = target,
        //.optimize = optimize,
    //});

    const stb = b.dependency("stb", .{
        .target = target,
        .optimize = optimize,
    });

    const exe_mod = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });

    const hook = b.addExecutable(.{
        .name = "hook",
        .root_module = exe_mod,
    });

    hook.linkLibC();
    hook.linkSystemLibrary("user32");
    hook.linkSystemLibrary("dwmapi");
    //hook.linkSystemLibrary("d3d9");

    const flags = &[_][]const u8 {
        "-Wall",
        "-Wextra",
        "-Wpedantic",
        "-Werror",
        "-std=c23",
    };

    //hook.addIncludePath(nuklear.path(""));
    //hook.addIncludePath(nuklear.path("demo/d3d9"));
    hook.addIncludePath(stb.path(""));
    
    hook.addCSourceFile(.{
        .file = b.path("src/hook.c"),
        .flags = flags,
    });
    
    // hook.subsystem = .Windows;

    b.installArtifact(hook);
}
