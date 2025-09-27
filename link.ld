/* Wrapper linker script for Foenix builds.
 *
 * The llvm-mos driver automatically appends "-Tlink.ld" to the linker
 * invocation. By keeping this thin wrapper at the project root we can redirect
 * that request to the in-repo Foenix script that lives under
 * `toolchain/linker/link.ld`.
 */
INCLUDE toolchain/linker/link.ld