#include "VideoCommon/TurbulenceTriggerCaption.h"
#include "turbulence_prompt_recipes.hpp"
#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>

int main(int argc, char** argv)
{
  using namespace TurbulencePromptTextures;
  const auto recipes = turbulence::BuildPromptRecipes();
  const auto find = [&](const char* name) -> const Recipe& {
    const auto it = std::find_if(recipes.begin(), recipes.end(), [&](const auto& r) {
      return r.name_prefix == name;
    });
    assert(it != recipes.end()); return *it;
  };
  assert(Detail::RecipePrefix("tex1_512x512_m", "6e2a2924575e9af0") ==
         "tex1_512x512_6e2a2924575e9af0");
  assert(Detail::RecipePrefix("tex1_256x128_m", "b59d2162deab7746") ==
         "tex1_256x128_b59d2162deab7746");
  const auto& drift = find("tex1_256x128_b59d2162deab7746");
  constexpr u32 width = 256, height = 128;
  std::vector<u8> original(width*height*4);
  for (u32 y=0;y<height;++y)
    for (u32 x=0;x<width;++x)
    {
      const size_t p=(y*width+x)*4;
      original[p]=x; original[p+1]=y; original[p+2]=(x+y)/2; original[p+3]=255;
    }
  if (argc > 1)
  {
    std::ifstream input(argv[1],std::ios::binary);
    input.read(reinterpret_cast<char*>(original.data()),original.size());
    assert(input.gcount() == static_cast<std::streamsize>(original.size()));
  }
  std::vector<u8> previous;
  for (Style style : {GameCube,Xbox,PlayStation})
  {
    auto image=original;
    Detail::ApplyCells(image,width,height,original,width,drift,style);
    if (style==GameCube) { assert(image==original); continue; }
    // Both modern controllers replace the COMPLETE octagonal GC stick cell.
    for (u32 y=0;y<64;++y)
      for (u32 x=0;x<64;++x)
        for (u32 c=0;c<4;++c)
          assert(image[(y*width+x)*4+c] == original[(y*width+x+64)*4+c]);
    // Preserve the original PS2 L/R lettering; only the binding digit changes.
    if (style == PlayStation)
      for (const auto& cell : drift.cells)
        if (!cell.labels[style].empty())
          for (u32 y=0; y<64; ++y)
            for (u32 x=0; x<35; ++x)
              for (u32 k=0; k<4; ++k)
                assert(image[((cell.y+y)*width+cell.x+x)*4+k] ==
                       original[((cell.source_y[style]+y)*width+cell.source_x[style]+x)*4+k]);
    // Arrow art, modern source keycaps and the plus symbol are untouched.
    for (u32 y=0;y<height;++y)
      for (u32 x=0;x<width;++x)
        if (!(x<64) && !(x>=192 && y<64))
          for (u32 c=0;c<4;++c)
            assert(image[(y*width+x)*4+c] == original[(y*width+x)*4+c]);
    if (!previous.empty()) assert(image!=previous);
    previous=image;
    if (argc>2)
    {
      std::ofstream output(std::string(argv[2])+(style==Xbox?"-xbox.rgba":"-ps.rgba"),std::ios::binary);
      output.write(reinterpret_cast<const char*>(image.data()),image.size());
    }
  }
  const auto& boost = find("tex1_512x512_6e2a2924575e9af0");
  std::vector<u8> hud(512*512*4);
  for (size_t i=0;i<hud.size();++i) hud[i]=static_cast<u8>((i/4)%512);
  if (argc>3)
  {
    std::ifstream input(argv[3],std::ios::binary);
    input.read(reinterpret_cast<char*>(hud.data()),hud.size());
    assert(input.gcount()==static_cast<std::streamsize>(hud.size()));
  }
  for (Style style : {GameCube,Xbox,PlayStation})
  {
    auto image=hud;
    Detail::ApplyCells(image,512,512,hud,512,boost,style);
    if (style==GameCube) { assert(image==hud); continue; }
    const u32 source=style==Xbox?376:401;
    for (u32 y=0;y<512;++y)
      for (u32 x=0;x<512;++x)
        for (u32 c=0;c<4;++c)
        {
          const bool first=x>=376 && x<401 && y<25;
          const bool second=x>=426 && x<451 && y<25;
          const u32 sx=first?source+x-376:second?source+x-426:x;
          assert(image[(y*512+x)*4+c] == hud[(y*512+sx)*4+c]);
        }
    if (argc>3)
    {
      std::ofstream output(std::string(argv[2])+(style==Xbox?"-boost-xbox.rgba":"-boost-ps.rgba"),std::ios::binary);
      output.write(reinterpret_cast<const char*>(image.data()),image.size());
    }
  }
  const auto& shake = find("tex1_128x128_4ee3d7b790310d80");
  std::vector<u8> stick(128*128*4);
  for (size_t i=0;i<stick.size();++i) stick[i]=static_cast<u8>((i/4)%128);
  for (Style style : {GameCube,Xbox,PlayStation})
  {
    auto patched=stick;
    Detail::ApplyCells(patched,128,128,stick,128,shake,style);
    for (u32 y=0;y<128;++y)
      for (u32 x=0;x<128;++x)
        for (u32 c=0;c<4;++c)
          assert(patched[(y*128+x)*4+c] == stick[(y*128+x+((style!=GameCube && x<48 && y<48)?48:0))*4+c]);
  }
  std::cout << "Controller atlas: native GC, modern sticks, PS/Xbox triggers and PS Circle passed\n";
}
