#include "VideoCommon/TurbulenceTriggerCaption.h"
#include "turbulence_prompt_recipes.hpp"
#include <map>
#include <cassert>
#include <iostream>
struct Artwork { u32 width, height; std::vector<u8> rgba; };
static std::map<std::string, Artwork> images;
namespace TurbulencePromptTextures {
void RegisterArtwork(std::string name, u32 width, u32 height, std::vector<u8> rgba) {
  assert(rgba.size()==size_t(width)*height*4);
  images.emplace(std::move(name), Artwork{width,height,std::move(rgba)});
}
}
#include "turbulence_xbox_artwork.hpp"
int main() {
 using namespace TurbulencePromptTextures;
 turbulence::RegisterXboxPromptArtwork();
 assert(images.size()==3);
 const auto digest=[](const std::vector<u8>& rgba) {
  uint64_t value=14695981039346656037ull;
  for(u8 b:rgba)value=(value^b)*1099511628211ull;
  return value;
 };
 // Golden digests computed independently from the decoded Xbox disc pixels.
 assert(digest(images.at("xbox-stick").rgba)==0xa6eb56a706945f16ull);
 assert(digest(images.at("xbox-left-trigger").rgba)==0x1893354c948cadcfull);
 assert(digest(images.at("xbox-right-trigger").rgba)==0x1893354c948cadcfull);
 // The original Xbox uses the same silhouette for both trigger positions.
 assert(images.at("xbox-left-trigger").rgba==images.at("xbox-right-trigger").rgba);
 const auto recipes=turbulence::BuildPromptRecipes();
 unsigned cells=0;
 for (const auto& recipe:recipes) for (const auto& c:recipe.cells) {
  if(c.artwork[Xbox].empty())continue;
  ++cells; const auto& art=images.at(c.artwork[Xbox]);
  std::vector<u8> atlas(512*512*4,123);
  Detail::ApplyArtwork(atlas,512,512,c,art.rgba,art.width,art.height);
  for(u32 y=0;y<512;++y)for(u32 x=0;x<512;++x)for(u32 k=0;k<4;++k) {
   const auto actual=atlas[(y*512+x)*4+k];
   if(x>=c.x&&x<c.x+c.width&&y>=c.y&&y<c.y+c.height) {
    const auto sx=(x-c.x)*art.width/c.width,sy=(y-c.y)*art.height/c.height;
    assert(actual==art.rgba[(sy*art.width+sx)*4+k]);
   } else assert(actual==123);
  }
  auto unchanged=atlas;
  Detail::ApplyArtwork(atlas,512,512,c,std::span<const u8>{},64,64);
  assert(atlas==unchanged);
 }
 assert(cells==4); // drift stick, both triggers and turbulence shake stick
 std::cout<<"Original Xbox artwork: four complete cells, transparent rims and untouched neighbours passed\n";
}
