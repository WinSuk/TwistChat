/*    
 *  PebbleType companion app for Pebble smartwatches
 *  Copyright (C) 2014 - WinSuk (winsuk@winsuk.net)
 * 
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 * 
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

package com.winsuk.pebbletype;

import java.util.ArrayList;
import java.util.UUID;

import org.json.JSONException;

import com.getpebble.android.kit.PebbleKit;
import com.getpebble.android.kit.util.PebbleDictionary;

import android.net.Uri;
import android.provider.ContactsContract;
import android.provider.ContactsContract.PhoneLookup;
import android.app.Activity;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.telephony.SmsManager;
import android.util.Log;

public class WatchCommunication extends BroadcastReceiver {
	private static final UUID PEBBLE_APP_UUID = UUID.fromString("16bb858b-fdde-479a-acd6-9b7678c147b0");
	private static final int KEY_REQUEST_THREAD_LIST = 0;
	private static final int KEY_THREAD_LIST = 1;
	private static final int KEY_SEND_MESSAGE = 2;
	private static final int KEY_MESSAGE_SENT = 3;
	private static final int KEY_MESSAGE_NOT_SENT = 4;
	private static final String ACTION_PEBBLE_RECEIVE = "com.getpebble.action.app.RECEIVE";
	private static final String ACTION_SMS_REPLY = "com.winsuk.pebbletype.sms.REPLY";
	
	@Override
	public void onReceive(Context context, Intent intent) {
		if (ACTION_PEBBLE_RECEIVE.equals(intent.getAction())) {
			final UUID receivedUuid = (UUID)intent.getSerializableExtra("uuid");
	
			// Pebble-enabled apps are expected to be good citizens and only inspect broadcasts containing their UUID
			if (!PEBBLE_APP_UUID.equals(receivedUuid)) {
				return;
			}
	
			final int transactionId = intent.getIntExtra("transaction_id", -1);
			final String jsonData = intent.getStringExtra("msg_data");
			if (jsonData == null || jsonData.isEmpty()) {
				return;
			}
	
			try {
				final PebbleDictionary data = PebbleDictionary.fromJson(jsonData);
				receiveData(context, transactionId, data);
			} catch (JSONException e) {
				e.printStackTrace();
				return;
			}
		}
		
		if (ACTION_SMS_REPLY.equals(intent.getAction())) {
			boolean sent = getResultCode() == Activity.RESULT_OK;
			
			if (!sent) Log.w("com.winsuk.pebbletype", "Message failed to send. Result code: " + getResultCode());
			
			PebbleDictionary data = new PebbleDictionary();
			data.addUint8(sent ? KEY_MESSAGE_SENT : KEY_MESSAGE_NOT_SENT, (byte)1);
			PebbleKit.sendDataToPebble(context, PEBBLE_APP_UUID, data);
		}
	}
	
	public void receiveData(final Context context, final int transactionId, final PebbleDictionary data) {
		PebbleKit.sendAckToPebble(context, transactionId);
		
		if (data.contains(KEY_REQUEST_THREAD_LIST)) {
			sendThreadList(context);
		}
		
		if (data.contains(KEY_SEND_MESSAGE)) {
			String str = data.getString(KEY_SEND_MESSAGE);
			boolean demoMode = str.startsWith(";");
			
			String address = str.substring(0, str.indexOf(";"));
			String message = str.substring(str.indexOf(";") +1, str.length());
			
			if (!demoMode) {
				SmsManager sms = SmsManager.getDefault();
				
				// Tried to use getService here which makes more sense, but that didn't work
				PendingIntent pi = PendingIntent.getBroadcast(context, 0, new Intent(ACTION_SMS_REPLY), PendingIntent.FLAG_ONE_SHOT);
				
				sms.sendTextMessage(address, null, message, pi, null);
			}
		}
	}
	
	private void sendThreadList(Context context) {
		Cursor msgCursor = context.getContentResolver().query(Uri.parse("content://sms/inbox"), null, null, null, null);
		Activity act = new Activity();
		act.startManagingCursor(msgCursor);
		ArrayList<SMSThread> threads = new ArrayList<SMSThread>();
		
		if(msgCursor.moveToFirst()){
			for (int i = 0; i < msgCursor.getCount(); i++) {
				int thread_id = msgCursor.getInt(msgCursor.getColumnIndexOrThrow("thread_id"));
				SMSThread th = null;
				for (int t = 0; t < threads.size(); t++) {
					SMSThread existingTh = threads.get(t);
					if (existingTh.thread_id == thread_id) {
						th = existingTh;
					}
				}
				
				if (th == null) {
					String address = msgCursor.getString(msgCursor.getColumnIndexOrThrow("address")).toString();
					String name = "";
					
					Uri uri = Uri.withAppendedPath(ContactsContract.PhoneLookup.CONTENT_FILTER_URI, address);
					Cursor contactCursor = context.getContentResolver().query(uri, new String[] {PhoneLookup.DISPLAY_NAME}, null, null, null );
					if(contactCursor.moveToFirst()) {
						name = contactCursor.getString(contactCursor.getColumnIndex(PhoneLookup.DISPLAY_NAME));
						contactCursor.close();
					}
					
					th = new SMSThread();
					th.thread_id = thread_id;
					th.address = address;
					th.name = name;
					th.messages = new ArrayList<String>();
					threads.add(th);
				}
				
				String body = msgCursor.getString(msgCursor.getColumnIndexOrThrow("body")).toString();
				if (th.messages.size() < 5) {
					th.messages.add(body);
				}
				
				msgCursor.moveToNext();
			}
		}
		msgCursor.close();
		
		String output = "";
		for (int i = 0; i < threads.size(); i++) {
			SMSThread thread = threads.get(i);
			if (thread.name == "") thread.name = thread.address;
			output = output + thread.address + ";" + thread.name + "\n";
			
			/* List messages
			for (int ii = 0; ii < thread.messages.size(); ii++) {
				output = output + thread.messages.get(ii) + "\n";
			}*/
		}
		PebbleDictionary data = new PebbleDictionary();
		data.addString(KEY_THREAD_LIST, output);
		PebbleKit.sendDataToPebble(context, PEBBLE_APP_UUID, data);
	}

}

class SMSThread {
	public int thread_id;
	public String address;
	public String name;
	public ArrayList<String> messages;
}
