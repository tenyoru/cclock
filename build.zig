const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const exe = b.addExecutable(.{
        .name = "cclock",
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
        }),
    });

    exe.root_module.addCSourceFile(.{
        .file = b.path("src/main.c"),
        .flags = &.{
            "-Wall",
            "-Wextra",
        },
    });

    exe.linkLibC();

    // Add GTK4 dependencies via pkg-config
    exe.linkSystemLibrary("gtk4");
    exe.linkSystemLibrary("gtk4-layer-shell-0");

    b.installArtifact(exe);

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());

    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const run_step = b.step("run", "Run the app");
    run_step.dependOn(&run_cmd.step);
}
