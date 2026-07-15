meta:
	ADDON_NAME = ofxTanim
	ADDON_DESCRIPTION = Timeline keyframe animation for openFrameworks ECS apps
	ADDON_AUTHOR = hegworks + OF integration
	ADDON_TAGS = "timeline" "animation" "keyframe" "imgui" "entt" "ecs"
	ADDON_URL = https://github.com/ofxyz/ofxTanim

common:
	ADDON_DEPENDENCIES = ofxEnTT ofxImGui ofxEnTTInspector

	# Explicitly list only src/ files — prevents any residual bundled libs
	# under libs/ from being auto-compiled by the OF build system.
	ADDON_SOURCES = src/bezier.cpp
	ADDON_SOURCES += src/curve_functions.cpp
	ADDON_SOURCES += src/sequencer.cpp
	ADDON_SOURCES += src/tanim.cpp
	ADDON_SOURCES += src/timeliner.cpp

	# Optional ofxKit glue (ofxAnimationKit). Guarded by
	# __has_include("Runtime.h") — compiles to empty TUs unless ofxKit is
	# in addons.make, so standalone ofxTanim apps are unaffected.
	ADDON_SOURCES += src/kit/AnimationBridge.cpp
	ADDON_SOURCES += src/kit/ofxAnimationKit.cpp
	ADDON_SOURCES += src/kit/TanimEditorSystem.cpp

	ADDON_INCLUDES += src
	ADDON_INCLUDES += src/kit
	ADDON_INCLUDES += libs/include
