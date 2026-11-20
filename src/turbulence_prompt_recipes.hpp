// Turbulence controller atlas recipes, shared by runtime and regression tests.
#pragma once
#include "VideoCommon/TurbulencePromptTextures.h"
namespace turbulence {
inline std::vector<TurbulencePromptTextures::Recipe> BuildPromptRecipes() {
  std::vector<TurbulencePromptTextures::Recipe> recipes;
  using TurbulencePromptTextures::CellCopy;
  using TurbulencePromptTextures::Recipe;
  // 256x128 HUD/menu sheet: A row (GC teal A, Xbox green A, PS cross, PS
  // triangle) at y 91, Y row (GC grey Y, Xbox gold Y, PS circle) at y 109.
  recipes.push_back(Recipe{
      "tex1_256x128_af289750027e9b53", 9,
      {CellCopy{1, 91, 19, 18, {1, 20, 39}, {91, 91, 91}},      // A -> A / cross
       CellCopy{1, 109, 19, 18, {1, 20, 58}, {109, 109, 91}}}}); // Y -> Y / triangle
  // 512x128 "Select / Back" bar: A row at y 0, B row at y 22, 24 px cells.
  recipes.push_back(Recipe{
      "tex1_512x128_585837ce16d8abc6", 9,
      {CellCopy{0, 0, 24, 22, {0, 24, 48}, {0, 0, 0}},          // A -> A / cross
       CellCopy{0, 22, 24, 22, {0, 24, 48}, {22, 22, 22}}}});   // B -> B / circle
  // Race drift hints use a separate CI4 trigger/stick sheet. These cells
  // are not part of either menu atlas above. Keep palette variants working.
  // The shipped PS artwork says L1/R1; our default bindings use analog
  // triggers. Xbox uses the original trigger silhouettes from its own disc;
  // PlayStation retains its original keycaps with binding-correct L2/R2 labels.
  recipes.push_back(Recipe{
      "tex1_256x128_b59d2162deab7746", 8,
      {CellCopy{0, 0, 64, 64, {0, 64, 64}, {0, 0, 0}, {}, {"", "xbox-stick", ""}}, // Original Xbox stick / PS2 rubber thumbstick
       CellCopy{0, 64, 64, 64, {0, 128, 128}, {64, 64, 64}, {"", "", "L2"}, {"", "xbox-left-trigger", ""}},
       CellCopy{192, 0, 64, 64, {192, 64, 64}, {0, 64, 64}, {"", "", "R2"}, {"", "xbox-right-trigger", ""}}}});
  // Turbulence's in-air shake cue uses another, smaller stick atlas.
  recipes.push_back(Recipe{
      "tex1_128x128_4ee3d7b790310d80", 8,
      {CellCopy{0, 0, 48, 48, {0, 48, 48}, {0, 0, 0}, {}, {"", "xbox-stick", ""}}}});
  // Race HUD slots: Xbox B at 376, PS Circle at 401, GameCube B at 426.
  // The atlas is shared across consoles; its first cell is NOT GameCube.
  // Cover both B aliases completely; a 24-pixel copy leaves the old rim.
  recipes.push_back(Recipe{
      "tex1_512x512_6e2a2924575e9af0", 9,
      {CellCopy{376, 0, 25, 25, {376, 376, 401}, {0, 0, 0}},
       CellCopy{426, 0, 25, 25, {426, 376, 401}, {0, 0, 0}}}});
  return recipes;
}
}  // namespace turbulence
