class Boost176 < Formula
  desc "Collection of portable C++ source libraries"
  homepage "https://www.boost.org/"
  url "https://boostorg.jfrog.io/artifactory/main/release/1.76.0/source/boost_1_76_0.tar.bz2"
  sha256 "f0397ba6e982c4450f27bf32a2a83292aba035b827a5623a14636ea583318c41"
  license "BSL-1.0"
  revision 6

  keg_only :versioned_formula

  depends_on "icu4c@76"

  uses_from_macos "bzip2"
  uses_from_macos "zlib"

  # Backport fixes for newer Clang
  patch :p2 do
    url "https://github.com/boostorg/numeric_conversion/commit/50a1eae942effb0a9b90724323ef8f2a67e7984a.patch?full_index=1"
    sha256 "d96761257f7efc2edc8414f1a2522fc07a3d7d56bb55a51d14af9abd39e389c8"
  end
  patch :p2 do
    url "https://github.com/boostorg/mpl/commit/b37b709cbdb6b2c285fb808dab985aa005786351.patch?full_index=1"
    sha256 "b8013ad3e6b63698158319f5efc2fe1558a00c1d2e32193086f741e774acc3e4"
  end

  def install
    # Force boost to compile with the desired compiler
    open("user-config.jam", "a") do |file|
      if OS.mac?
        file.write "using darwin : : #{ENV.cxx} ;\n"
      else
        file.write "using gcc : : #{ENV.cxx} ;\n"
      end
    end

    # libdir should be set by --prefix but isn't
    icu4c_prefix = Formula["icu4c@74"].opt_prefix
    bootstrap_args = %W[
      --prefix=#{prefix}
      --libdir=#{lib}
      --with-icu=#{icu4c_prefix}
    ]

    # Handle libraries that will not be built.
    without_libraries = ["python", "mpi"]

    # Boost.Log cannot be built using Apple GCC at the moment. Disabled
    # on such systems.
    without_libraries << "log" if ENV.compiler == :gcc

    bootstrap_args << "--without-libraries=#{without_libraries.join(",")}"

    # layout should be synchronized with boost-python and boost-mpi
    args = %W[
      --prefix=#{prefix}
      --libdir=#{lib}
      -d2
      -j#{ENV.make_jobs}
      --layout=tagged-1.66
      --user-config=user-config.jam
      -sNO_LZMA=1
      -sNO_ZSTD=1
      install
      threading=multi,single
      link=shared,static
    ]

    # Boost is using "clang++ -x c" to select C compiler which breaks C++14
    # handling using ENV.cxx14. Using "cxxflags" and "linkflags" still works.
    args << "cxxflags=-std=c++14"
    args << "cxxflags=-stdlib=libc++" << "linkflags=-stdlib=libc++" if ENV.compiler == :clang

    system "./bootstrap.sh", *bootstrap_args
    system "./b2", "headers"
    system "./b2", *args
  end

  test do
    (testpath/"test.cpp").write <<~CPP
      #include <boost/algorithm/string.hpp>
      #include <string>
      #include <vector>
      #include <assert.h>
      using namespace boost::algorithm;
      using namespace std;

      int main()
      {
        string str("a,b");
        vector<string> strVec;
        split(strVec, str, is_any_of(","));
        assert(strVec.size()==2);
        assert(strVec[0]=="a");
        assert(strVec[1]=="b");
        return 0;
      }
    CPP
    system ENV.cxx, "-I#{Formula["boost@1.76"].opt_include}", "test.cpp", "-std=c++14", "-o", "test"
    system "./test"
  end
end
