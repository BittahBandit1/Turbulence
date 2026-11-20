#pragma once
#include "turbulence_native_glyphs.hpp"
#include "VideoCommon/TurbulenceMenuArtwork.h"
#include "imgui.h"
#include <cstring>

namespace turbulence {
class NativeMenuText {
  ImFontAtlasRectId m_rect[3]={ImFontAtlasRectId_Invalid,ImFontAtlasRectId_Invalid,ImFontAtlasRectId_Invalid};
public:
  bool Ready() {
    auto* atlas=ImGui::GetIO().Fonts;
    for(int i=0;i<3;++i) {
      const auto& source=TurbulenceMenuArtwork::font_atlases[i];
      if(m_rect[i]!=ImFontAtlasRectId_Invalid || !source) continue;
      const int w=i?1024:512,h=i?64:512;
      ImFontAtlasRect rect;
      m_rect[i]=atlas->AddCustomRect(w,h,&rect);
      if(m_rect[i]==ImFontAtlasRectId_Invalid) continue;
      auto* tex=atlas->TexData;
      if(tex->BytesPerPixel!=4) return false;
      for(int y=0;y<h;++y)
        std::memcpy(tex->GetPixelsAt(rect.x,rect.y+y),source->data()+y*w*4,w*4);
      tex->UseColors=true;
    }
    return m_rect[0]!=ImFontAtlasRectId_Invalid;
  }
  const NativeGlyph* Glyph(unsigned char ch) const {
    for(const auto& g:kNativeGlyphs) if(g.ch==ch) {
      if(g.atlas==1 && !TurbulenceMenuArtwork::font_atlases[1])
        for(const auto& fallback:kNativeGlyphsBfd) if(fallback.ch==ch) return &fallback;
      return &g;
    }
    for(const auto& g:kNativeGlyphs) if(g.ch=='?') return &g;
    return nullptr;
  }
  float Width(const char* text,float size) const {
    float result=0;
    for(const unsigned char* c=reinterpret_cast<const unsigned char*>(text);*c;++c)
      if(const auto* g=Glyph(*c)) result+=g->advance*size/24.f;
    return result;
  }
  void Draw(ImDrawList* draw,ImVec2 p,float size,float sx,float sy,ImU32 color,
            const char* text,float max_width=0) {
    if(!Ready()) return;
    float scale=size/24.f;
    const float width=Width(text,size);
    if(max_width>0 && width>max_width) scale*=max_width/width;
    auto* atlas=ImGui::GetIO().Fonts;
    for(const unsigned char* c=reinterpret_cast<const unsigned char*>(text);*c;++c) {
      const auto* g=Glyph(*c);
      if(!g) continue;
      ImFontAtlasRect rect;
      if(g->w && atlas->GetCustomRect(m_rect[g->atlas],&rect)) {
        const auto* tex=atlas->TexData;
        const ImVec2 uv0{float(rect.x+g->x)/tex->Width,float(rect.y+g->y)/tex->Height};
        const ImVec2 uv1{float(rect.x+g->x+g->w)/tex->Width,float(rect.y+g->y+g->h)/tex->Height};
        const float dy=g->atlas?2*scale*sy:0;
        draw->AddImage(atlas->TexData->GetTexRef(),{p.x,p.y+dy},
          {p.x+g->w*scale*sx,p.y+dy+g->h*scale*sy},uv0,uv1,color);
      }
      p.x+=g->advance*scale*sx;
    }
  }
};
}
