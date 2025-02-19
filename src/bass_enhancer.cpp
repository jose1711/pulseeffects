/*
 *  Copyright © 2017-2020 Wellington Wallace
 *
 *  This file is part of PulseEffects.
 *
 *  PulseEffects is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  PulseEffects is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with PulseEffects.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "bass_enhancer.hpp"
#include <glibmm/main.h>
#include "util.hpp"

namespace {

void on_post_messages_changed(GSettings* settings, gchar* key, BassEnhancer* l) {
  const auto post = g_settings_get_boolean(settings, key);

  if (post) {
    if (!l->harmonics_connection.connected()) {
      l->harmonics_connection = Glib::signal_timeout().connect(
          [l]() {
            float harmonics = 0.0F;

            g_object_get(l->bass_enhancer, "meter-drive", &harmonics, nullptr);

            l->harmonics.emit(harmonics);

            return true;
          },
          100);
    }
  } else {
    l->harmonics_connection.disconnect();
  }
}

}  // namespace

BassEnhancer::BassEnhancer(const std::string& tag, const std::string& schema, const std::string& schema_path)
    : PluginBase(tag, "bass_enhancer", schema, schema_path) {
  bass_enhancer = gst_element_factory_make("calf-sourceforge-net-plugins-BassEnhancer", nullptr);

  if (is_installed(bass_enhancer)) {
    auto* in_level = gst_element_factory_make("level", "bass_enhancer_input_level");
    auto* out_level = gst_element_factory_make("level", "bass_enhancer_output_level");
    auto* audioconvert_in = gst_element_factory_make("audioconvert", "bass_enhancer_audioconvert_in");
    auto* audioconvert_out = gst_element_factory_make("audioconvert", "bass_enhancer_audioconvert_out");

    gst_bin_add_many(GST_BIN(bin), in_level, audioconvert_in, bass_enhancer, audioconvert_out, out_level, nullptr);

    gst_element_link_many(in_level, audioconvert_in, bass_enhancer, audioconvert_out, out_level, nullptr);

    auto* pad_sink = gst_element_get_static_pad(in_level, "sink");
    auto* pad_src = gst_element_get_static_pad(out_level, "src");

    gst_element_add_pad(bin, gst_ghost_pad_new("sink", pad_sink));
    gst_element_add_pad(bin, gst_ghost_pad_new("src", pad_src));

    gst_object_unref(GST_OBJECT(pad_sink));
    gst_object_unref(GST_OBJECT(pad_src));

    g_object_set(bass_enhancer, "bypass", 0, nullptr);

    bind_to_gsettings();

    g_signal_connect(settings, "changed::post-messages", G_CALLBACK(on_post_messages_changed), this);

    g_settings_bind(settings, "post-messages", in_level, "post-messages", G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(settings, "post-messages", out_level, "post-messages", G_SETTINGS_BIND_DEFAULT);

    if (g_settings_get_boolean(settings, "state") != 0) {
      enable();
    }
  }
}

BassEnhancer::~BassEnhancer() {
  util::debug(log_tag + name + " destroyed");
}

void BassEnhancer::bind_to_gsettings() {
  g_settings_bind_with_mapping(settings, "input-gain", bass_enhancer, "level-in", G_SETTINGS_BIND_DEFAULT,
                               util::db20_gain_to_linear, util::linear_gain_to_db20, nullptr, nullptr);

  g_settings_bind_with_mapping(settings, "output-gain", bass_enhancer, "level-out", G_SETTINGS_BIND_DEFAULT,
                               util::db20_gain_to_linear, util::linear_gain_to_db20, nullptr, nullptr);

  g_settings_bind_with_mapping(settings, "amount", bass_enhancer, "amount", G_SETTINGS_BIND_DEFAULT,
                               util::db20_gain_to_linear, util::linear_gain_to_db20, nullptr, nullptr);

  g_settings_bind_with_mapping(settings, "harmonics", bass_enhancer, "drive", G_SETTINGS_BIND_GET,
                               util::double_to_float, nullptr, nullptr, nullptr);

  g_settings_bind_with_mapping(settings, "scope", bass_enhancer, "freq", G_SETTINGS_BIND_GET, util::double_to_float,
                               nullptr, nullptr, nullptr);

  g_settings_bind_with_mapping(settings, "floor", bass_enhancer, "floor", G_SETTINGS_BIND_GET, util::double_to_float,
                               nullptr, nullptr, nullptr);

  g_settings_bind_with_mapping(settings, "blend", bass_enhancer, "blend", G_SETTINGS_BIND_GET, util::double_to_float,
                               nullptr, nullptr, nullptr);

  g_settings_bind(settings, "floor-active", bass_enhancer, "floor-active", G_SETTINGS_BIND_DEFAULT);

  g_settings_bind(settings, "listen", bass_enhancer, "listen", G_SETTINGS_BIND_DEFAULT);
}
