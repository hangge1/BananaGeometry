#ifndef DRAWER_TESTS_SUPPORT_GALLERY_FIXTURE_H_
#define DRAWER_TESTS_SUPPORT_GALLERY_FIXTURE_H_

#include "support/session_fixture.h"

namespace drawer::testing {
// Populate explicitly: production Gallery opens empty in drawing mode.
inline void PrepareGallery(Session* session) {
  ExecuteChecked(session,
                 "ctx:edit('reference "
                 "scene',function()require('sample_scene').populate(ctx)end)");
  session->Restore(session->SaveData());
  ExecuteChecked(session, "ctx:set_mode('edit')");
}
struct GalleryFixture {
  Session session{SourceRoot() / "sdk/drawer.lua",
                  SourceRoot() / "cases/gallery/main.lua"};
  GalleryFixture() { PrepareGallery(&session); }
};
}  // namespace drawer::testing

#endif  // DRAWER_TESTS_SUPPORT_GALLERY_FIXTURE_H_
