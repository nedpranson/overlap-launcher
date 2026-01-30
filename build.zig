const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
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
    exe.linkSystemLibrary("d3d9");

    const flags = &[_][]const u8 {};

    exe.addIncludePath(nuklear.path(""));
    exe.addIncludePath(nuklear.path("demo/d3d9"));
    
    exe.addCSourceFile(.{
        .file = b.path("src/main.c"),
        .flags = flags,
    });
    
    b.installArtifact(exe);
}
