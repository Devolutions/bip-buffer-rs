
fn main() {
    cc::Build::new()
        .file("src/bip_buffer.c")
        .compile("bip_buffer");
}
