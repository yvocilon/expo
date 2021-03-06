package expo.modules.notifications.notifications.handling;

import android.content.Context;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.ResultReceiver;

import org.json.JSONObject;
import org.unimodules.core.ModuleRegistry;
import org.unimodules.core.Promise;
import org.unimodules.core.interfaces.services.EventEmitter;

import expo.modules.notifications.notifications.NotificationSerializer;
import expo.modules.notifications.notifications.interfaces.NotificationBehavior;
import expo.modules.notifications.notifications.interfaces.NotificationTrigger;
import expo.modules.notifications.notifications.service.BaseNotificationsService;

/**
 * A "task" responsible for managing response to a single notification.
 */
/* package */ class SingleNotificationHandlerTask {
  /**
   * {@link Handler} on which lifecycle events are executed.
   */
  private final static Handler HANDLER = new Handler(Looper.getMainLooper());

  /**
   * Name of the event asking the delegate for behavior.
   */
  private final static String HANDLE_NOTIFICATION_EVENT_NAME = "onHandleNotification";
  /**
   * Name of the event emitted if the delegate doesn't respond in time.
   */
  private final static String HANDLE_NOTIFICATION_TIMEOUT_EVENT_NAME = "onHandleNotificationTimeout";

  /**
   * Seconds since sending the {@link #HANDLE_NOTIFICATION_EVENT_NAME} until the task
   * is considered timed out.
   */
  private final static int SECONDS_TO_TIMEOUT = 3;

  private Context mContext;
  private EventEmitter mEventEmitter;
  private JSONObject mNotificationRequest;
  private NotificationTrigger mNotificationTrigger;
  private NotificationBehavior mBehavior;
  private NotificationsHandler mDelegate;
  private String mIdentifier;

  private Runnable mTimeoutRunnable = new Runnable() {
    @Override
    public void run() {
      SingleNotificationHandlerTask.this.handleTimeout();
    }
  };

  /* package */ SingleNotificationHandlerTask(Context context, ModuleRegistry moduleRegistry, String identifier, JSONObject notificationRequest, NotificationTrigger trigger, NotificationsHandler delegate) {
    mContext = context;
    mEventEmitter = moduleRegistry.getModule(EventEmitter.class);
    mNotificationRequest = notificationRequest;
    mNotificationTrigger = trigger;
    mDelegate = delegate;

    mIdentifier = identifier;
  }

  /**
   * @return Identifier of the task.
   */
  /* package */ String getIdentifier() {
    return mIdentifier;
  }

  /**
   * Starts the task, i.e. sends an event to the app's delegate and starts a timeout
   * after which the task finishes itself.
   */
  /* package */ void start() {
    Bundle eventBody = new Bundle();
    eventBody.putString("id", getIdentifier());
    eventBody.putBundle("notification", NotificationSerializer.toBundle(mIdentifier, mNotificationRequest, mNotificationTrigger));
    mEventEmitter.emit(HANDLE_NOTIFICATION_EVENT_NAME, eventBody);

    HANDLER.postDelayed(mTimeoutRunnable, SECONDS_TO_TIMEOUT * 1000);
  }

  /**
   * Stops the task abruptly (in case the app is being destroyed and there is no reason
   * to wait for the response anymore).
   */
  /* package */ void stop() {
    finish();
  }

  /**
   * Informs the task of a response - behavior requested by the app.
   *
   * @param behavior Behavior requested by the app
   * @param promise  Promise to fulfill once the behavior is applied to the notification.
   */
  /* package */ void handleResponse(NotificationBehavior behavior, final Promise promise) {
    mBehavior = behavior;
    HANDLER.post(new Runnable() {
      @Override
      public void run() {
        BaseNotificationsService.enqueuePresent(mContext, getIdentifier(), mNotificationRequest, mBehavior, new ResultReceiver(HANDLER) {
          @Override
          protected void onReceiveResult(int resultCode, Bundle resultData) {
            super.onReceiveResult(resultCode, resultData);
            if (resultCode == BaseNotificationsService.SUCCESS_CODE) {
              promise.resolve(null);
            } else {
              Exception e = resultData.getParcelable(BaseNotificationsService.EXCEPTION_KEY);
              promise.reject("ERR_NOTIFICATION_PRESENTATION_FAILED", "Notification presentation failed.", e);
            }
          }
        });
        finish();
      }
    });
  }

  /**
   * Callback called by {@link #mTimeoutRunnable} after timeout time elapses.
   * <p>
   * Sends a timeout event to the app.
   */
  private void handleTimeout() {
    Bundle eventBody = new Bundle();
    eventBody.putString("id", getIdentifier());
    eventBody.putBundle("notification", NotificationSerializer.toBundle(mIdentifier, mNotificationRequest, mNotificationTrigger));
    mEventEmitter.emit(HANDLE_NOTIFICATION_TIMEOUT_EVENT_NAME, eventBody);

    finish();
  }

  /**
   * Callback called when the task fulfills its responsibility. Clears up {@link #HANDLER}
   * and informs {@link #mDelegate} of the task's state.
   */
  private void finish() {
    HANDLER.removeCallbacks(mTimeoutRunnable);
    mDelegate.onTaskFinished(this);
  }
}
