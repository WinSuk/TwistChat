package com.winsuk.pebbletype;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

public class StartupReceiver extends BroadcastReceiver {
	@Override
	public void onReceive(Context context, Intent intent) {
		// Do nothing - just want it to start up so we can get com.getpebble.action.app.RECEIVE
	}
} 