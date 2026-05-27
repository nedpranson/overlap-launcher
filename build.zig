const std = @import("std");

// todo: if x64 is compiled compile hook x86 ver

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{ .default_target = .{ .os_tag = .windows } });
    const optimize = b.standardOptimizeOption(.{});

    const clay = b.dependency("clay", .{
        .target = target,
        .optimize = optimize,
    });

    const onecore = b.dependency("onecore", .{
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

    // const hook_mod = b.createModule(.{
    //     .target = target,
    //     .optimize = optimize,
    //     .link_libc = true,
    // });
    //
    // hook_mod.linkSystemLibrary("user32", .{});
    // hook_mod.linkSystemLibrary("dwmapi", .{});
    //
    // hook_mod.addCSourceFile(.{
    //     .file = b.path("src/hook.c"),
    //     .flags = flags,
    // });
    //
    // const hook = b.addExecutable(.{
    //     .name = "hook",
    //     .root_module = hook_mod,
    // });
    //
    // b.installArtifact(hook);
    
    const overlap_mod = b.createModule(.{
        .target = target,
        .optimize = optimize,
        .link_libc = true,
    });

    overlap_mod.linkSystemLibrary("user32", .{});
    overlap_mod.linkSystemLibrary("dwmapi", .{});
    overlap_mod.linkSystemLibrary("d3d9", .{});
    overlap_mod.linkSystemLibrary("dwrite", .{});

    //b.addSystemCommand()

    overlap_mod.addIncludePath(clay.path(""));
    overlap_mod.addIncludePath(onecore.path(""));

    overlap_mod.addCSourceFile(.{
        .file = b.path("src/app.c"),
        .flags = flags,
    });

    const overlap = b.addExecutable(.{
        .name = "overlap",
        .root_module = overlap_mod,
    });

    overlap.subsystem = .Windows;

    b.installArtifact(overlap);

    const run_step = b.step("run", "Run the app");

    const run_cmd = b.addRunArtifact(overlap);
    run_step.dependOn(&run_cmd.step);

    run_cmd.step.dependOn(b.getInstallStep());

    if (b.args) |args| {
        run_cmd.addArgs(args);
    }
}
