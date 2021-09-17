#ifndef	_NBL_SYSTEM_C_APPLICATION_FRAMEWORK_ANDROID_H_INCLUDED_
#define	_NBL_SYSTEM_C_APPLICATION_FRAMEWORK_ANDROID_H_INCLUDED_
#ifdef _NBL_PLATFORM_ANDROID_
#include "nbl/core/declarations.h"
#include <android_native_app_glue.h>
#include <android/sensor.h>
#include <android/log.h>
namespace nbl::system
{

    class CApplicationFrameworkAndroid : public core::IReferenceCounted
    {
    public:
        void onStateSaved(android_app* params)
        {
            return onStateSaved_impl(params);
        }
        void onWindowInitialized(android_app* params)
        {
            return onWindowInitialized_impl(params);
        }
        void onWindowTerminated(android_app* params)
        {
            return onWindowTerminated_impl(params);
            auto wnd = ((SContext*)params->userData)->window;
            auto eventCallback = wnd->getEventCallback();
            [[maybe_unused]] bool ok = eventCallback->onWindowClosed(wnd.get());
            //TODO other callbacks;
        }
        void onFocusGained(android_app* params)
        {
            return onFocusGained_impl(params);
        }
        void onFocusLost(android_app* params)
        {
            return onFocusLost_impl(params);
        }
        virtual void workLoopBody(android_app* params) = 0;
    protected:
        virtual void onStateSaved_impl(android_app* params) {}
        virtual void onWindowInitialized_impl(android_app* params) {}
        virtual void onWindowTerminated_impl(android_app* params) {}
        virtual void onFocusGained_impl(android_app* params) {}
        virtual void onFocusLost_impl(android_app* params) {}

    public:
        struct SSavedState {
            float angle;
            int32_t x;
            int32_t y;
        };
        struct SContext
        {
            SSavedState* state;
            CApplicationFrameworkAndroid* framework;
            core::smart_refctd_ptr<nbl::ui::IWindow> window;
            void* userData;
        };
    public:
        CApplicationFrameworkAndroid(android_app* params)
        {
            params->onAppCmd = handleCommand;
            params->onInputEvent = handleInput;
            ((SContext*)params->userData)->framework = this;
        }

        static int32_t handleInput(android_app* app, AInputEvent* event) {
            auto* framework = ((SContext*)app->userData)->framework;
            SContext* engine = (SContext*)app->userData;
            if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
                engine->state->x = AMotionEvent_getX(event, 0);
                engine->state->y = AMotionEvent_getY(event, 0);
                return 1;
            }
            return 0;
        }
        static void handleCommand(android_app* app, int32_t cmd) {
            auto* framework = ((SContext*)app->userData)->framework;
            auto* usrData = (SContext*)app->userData;
            switch (cmd) {
            case APP_CMD_SAVE_STATE:
                // The system has asked us to save our current state.  Do so.
                usrData->state = (SSavedState*)malloc(sizeof(SSavedState));
                *((SSavedState*)app->savedState) = *usrData->state;
                app->savedStateSize = sizeof(SSavedState);
                framework->onStateSaved(app);
                break;
            case APP_CMD_INIT_WINDOW:
                //debug_break();
                // The window is being shown, get it ready.
               /* if (app->window != nullptr) {
                    engine_init_display(engine);
                    engine_draw_frame(engine);
                }*/
                framework->onWindowInitialized(app);
                break;
            case APP_CMD_TERM_WINDOW:
                // The window is being hidden or closed, clean it up.
                //engine_term_display(engine);
                framework->onWindowTerminated(app);
                break;

            case APP_CMD_GAINED_FOCUS:
                // When our app gains focus, we start monitoring the accelerometer.
                //if (engine->accelerometerSensor != nullptr) {
                //    ASensorEventQueue_enableSensor(engine->sensorEventQueue,
                //                                   engine->accelerometerSensor);
                //    // We'd like to get 60 events per second (in us).
                //    ASensorEventQueue_setEventRate(engine->sensorEventQueue,
                //                                   engine->accelerometerSensor,
                //                                   (1000L/60)*1000);
                //}
                framework->onFocusGained(app);
                break;
            case APP_CMD_LOST_FOCUS:
                // When our app loses focus, we stop monitoring the accelerometer.
                // This is to avoid consuming battery while not being used.
                //if (engine->accelerometerSensor != nullptr) {
                //    ASensorEventQueue_disableSensor(engine->sensorEventQueue,
                //                                    engine->accelerometerSensor);
                //}
                //// Also stop animating.
                //engine->animating = 0;
                //engine_draw_frame(engine);
                framework->onFocusLost(app);
                break;

            default:
                break;
            }
        }
    };

}
// ... are the window event callback optional ctor params;
#define NBL_ANDROID_MAIN(android_app_class, user_data_type, window_event_callback, ...) void android_main(android_app* app){\
    user_data_type engine{};\
    nbl::system::CApplicationFrameworkAndroid::SContext ctx{};\
    ctx.userData = &engine;\
    app->userData = &ctx;\
    auto framework = nbl::core::make_smart_refctd_ptr<android_app_class>(app);\
    auto wndManager = nbl::core::make_smart_refctd_ptr<nbl::ui::CWindowManagerAndroid>(app);\
    nbl::ui::IWindow::SCreationParams params;\
    params.callback = nbl::core::make_smart_refctd_ptr<window_event_callback>(__VA_ARGS__);\
    auto wnd = wndManager->createWindow(std::move(params));\
    ctx.window = core::smart_refctd_ptr(wnd);\
    if (app->savedState != nullptr) {\
        ctx.state = (nbl::system::CApplicationFrameworkAndroid::SSavedState*)app->savedState;\
    }\
    while (true) {\
    int ident;\
    int events;\
    android_poll_source* source;\
    while ((ident = ALooper_pollAll(0, nullptr, &events, (void**)&source)) >= 0) {\
        LOGI("Entered poll loop iteration!");\
        if (source != nullptr) {\
            source->process(app, source);\
        }\
        if (app->destroyRequested != 0) {\
            framework->onWindowTerminated(app);\
            return;\
        }\
    }\
    \
    framework->workLoopBody(app);\
    }\
}
#endif
#endif