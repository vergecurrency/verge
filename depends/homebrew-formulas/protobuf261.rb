class Protobuf261 < Formula
  desc "Protocol buffers - Google data interchange format"
  homepage "https://github.com/google/protobuf/"

  url "https://github.com/google/protobuf/releases/download/v2.6.1/protobuf-2.6.1.tar.bz2"
  sha256 "ee445612d544d885ae240ffbcbf9267faa9f593b7b101f21d58beceb92661910"

  # Fixes the unexpected identifier error when compiling software against protobuf:
  # https://github.com/google/protobuf/issues/549
  patch :p1, :DATA

  conflicts_with "protobuf", :because => "Differing versions of same formula"

  # this will double the build time approximately if enabled
  option "with-check", "Run build-time check"

  def install
    # Don't build in debug mode. See:
    # https://github.com/Homebrew/homebrew/issues/9279
    # http://code.google.com/p/protobuf/source/browse/trunk/configure.ac#61
    ENV.prepend "CXXFLAGS", "-DNDEBUG"

    system "./configure", "--disable-debug", "--disable-dependency-tracking",
           "--prefix=#{prefix}",
           "--with-zlib"
    system "make"
    system "make", "check" if (build.with? "check") || build.bottle?
    system "make", "install"

    # Install editor support and examples
    doc.install "editors", "examples"
  end

  def caveats; <<-EOS
    Editor support and examples have been installed to:
      #{doc}
    EOS
  end

  test do
    testdata =
      <<-EOS
        package test;
        message TestCase {
          required string name = 4;
        }
        message Test {
          repeated TestCase case = 1;
        }
        EOS
    (testpath/"test.proto").write(testdata)
    system bin/"protoc", "test.proto", "--cpp_out=."
  end
end

__END__
diff --git a/src/google/protobuf/descriptor.h b/src/google/protobuf/descriptor.h
index 67afc77..504d5fe 100644
--- a/src/google/protobuf/descriptor.h
+++ b/src/google/protobuf/descriptor.h
@@ -59,6 +59,9 @@
 #include <vector>
 #include <google/protobuf/stubs/common.h>

+#ifdef TYPE_BOOL
+#undef TYPE_BOOL
+#endif

 namespace google {
 namespace protobuf {
