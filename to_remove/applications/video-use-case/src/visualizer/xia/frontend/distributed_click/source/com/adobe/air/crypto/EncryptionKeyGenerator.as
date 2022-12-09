/*
  Copyright (c) 2009, Adobe Systems Incorporated
  All rights reserved.

  Redistribution and use in source and binary forms, with or without 
  modification, are permitted provided that the following conditions are
  met:

  * Redistributions of source code must retain the above copyright notice, 
    this list of conditions and the following disclaimer.
  
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the 
    documentation and/or other materials provided with the distribution.
  
  * Neither the name of Adobe Systems Incorporated nor the names of its 
    contributors may be used to endorse or promote products derived from 
    this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
  IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR 
  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

package com.adobe.air.crypto
{
	import com.adobe.crypto.SHA256;
	
	import flash.data.EncryptedLocalStore;
	import flash.utils.ByteArray;
			
	/**
	 * The EncryptionKeyGenerator class generates an encryption key value, such as you would use 
	 * to encrypt files or data. For example, the encryption key is suitable to use as 
	 * an encryption key for an encrypted AIR local SQL (SQLite) database.
	 * 
	 * <p>This class uses techniques and algorithms that are designed for maximum 
	 * data privacy and security. Use this class to generate an encryption key if your 
	 * application requires data to be encrypted on a per-user level (in other words, 
	 * if only one user of the application should be able to access his or her data). 
	 * In some situations you may also want to use per-user encryption for data even 
	 * if the application design specifies that other users can access the data. For more 
	 * information, see 
	 * "<a href="http://help.adobe.com/en_US/AIR/1.5/devappsflex/WS34990ABF-C893-47ec-B813-9C9D9587A398.html">Considerations for using encryption with a database</a>" 
	 * in the guide 
	 * "<a href="http://help.adobe.com/en_US/AIR/1.5/devappsflex/">Developing Adobe AIR Applications with Flex</a>."</p>
	 * 
	 * <p>The generated encryption key is based on a password that you provide. For any given 
	 * password, in the same AIR application 
	 * running in the same user account on the same machine, the encryption key result is 
	 * the same.</p>
	 * 
	 * <p>To generate an encryption key from a password, use the <code>getEncryptionKey()</code> 
	 * method. To confirm that a password is a "strong" password before calling the 
	 * <code>getEncryptionKey()</code> method, use the <code>validateStrongPassword()</code> 
	 * method.</p>
	 * 
	 * <p>In addition, the EncryptionKeyGenerator includes a utility constant, 
	 * <code>ENCRYPTED_DB_PASSWORD_ERROR_ID</code>. This constant matches the error ID of 
	 * the SQLError error that occurs when code that is attempting to open an encrypted database 
	 * provides the wrong encryption key.</p>
	 * 
	 * <p>This class is designed to create an encryption key suitable for providing the highest 
	 * level of data privacy and security. In order to achieve that level of security, a few 
	 * security principles must be followed:</p>
	 * 
	 * <ul>
	 *   <li>Your application should never store the user-entered password</li>
	 *   <li>Your application should never store the encryption key returned by the 
	 *       <code>getEncryptionKey()</code> method.</li>
	 *   <li>Instead, each time the user runs the application and attempts to access the database, 
	 *       your application code should call the <code>getEncryptionKey()</code> method to 
	 *       regenerate the encryption key.</li>
	 * </ul>
	 * 
	 * <p>For more information about data security, and an explanation of the security techniques 
	 * used in the EncryptionKeyGenerator class, see 
	 * "<a href="http://help.adobe.com/en_US/AIR/1.5/devappsflex/WS61068DCE-9499-4d40-82B8-B71CC35D832C.html">Example: Generating and using an encryption key</a>" 
	 * in the guide 
	 * "<a href="http://help.adobe.com/en_US/AIR/1.5/devappsflex/">Developing Adobe AIR Applications with Flex</a>."</p>
	 */
	public class EncryptionKeyGenerator
	{
		// ------- Constants -------
		/**
		 * This constant matches the error ID (3138) of the SQLError error that occurs when 
		 * code that is attempting to open an encrypted database provides the wrong 
		 * encryption key.
		 */
		public static const ENCRYPTED_DB_PASSWORD_ERROR_ID:uint = 3138;
		
		private static const STRONG_PASSWORD_PATTERN:RegExp = /(?=^.{8,32}$)((?=.*\d)|(?=.*\W+))(?![.\n])(?=.*[A-Z])(?=.*[a-z]).*$/;
		private static const SALT_ELS_KEY:String = "com.adobe.air.crypto::EncryptedDBSalt$$$";
		
		
		// ------- Constructor -------
		
		/**
		 * Creates a new EncryptionKeyGenerator instance.
		 */
		public function EncryptionKeyGenerator() {}
		
		
		// ------- Public methods -------
		
		/**
		 * Checks a password and returns a value indicating whether the password is a "strong" 
		 * password. The criteria for a strong password are:
		 * 
		 * <ul>
		 *   <li>Minimum 8 characters</li>
		 *   <li>Maxmium 32 characters</li>
		 *   <li>Contains at least one lowercase letter</li>
		 *   <li>Contains at least one uppercase letter</li>
		 *   <li>Contains at least one number or symbol character</li>
		 * </ul>
		 * 
		 * @param password The password to check
		 * 
		 * @return A value indicating whether the password is a strong password (<code>true</code>) 
		 * or not (<code>false</code>).
		 */
		public function validateStrongPassword(password:String):Boolean
		{
			if (password == null || password.length <= 0)
			{
				return false;
			}
			
			return STRONG_PASSWORD_PATTERN.test(password);
		}
		
		
		/**
		 * Uses a password to generate a 16-byte encryption key. The return value is suitable 
		 * to use as an encryption key for an encrypted AIR local SQL (SQLite) database.
		 * 
		 * <p>For any given 
		 * password, calling the <code>getEncryptionKey()</code> method from the same AIR application 
		 * running in the same user account on the same machine, the encryption key result is 
		 * the same.</p>
		 * 
		 * <p>This method is designed to create an encryption key suitable for providing the highest 
		 * level of data privacy and security. In order to achieve that level of security, your 
		 * application must follow several security principles:</p>
		 * 
		 * <ul>
		 *   <li>Your application can never store the user-entered password</li>
		 *   <li>Your application can never store the encryption key returned by the 
		 *       <code>getEncryptionKey()</code> method.</li>
		 *   <li>Instead, each time the user runs the application and attempts to access the database, 
		 *       call the <code>getEncryptionKey()</code> method to regenerate the encryption key.</li>
		 * </ul>
		 * 
		 * <p>For more information about data security, and an explanation of the security techniques 
		 * used in the EncryptionKeyGenerator class, see 
		 * "<a href="http://help.adobe.com/en_US/AIR/1.5/devappsflex/WS61068DCE-9499-4d40-82B8-B71CC35D832C.html">Example: Generating and using an encryption key</a>" 
		 * in the guide 
		 * "<a href="http://help.adobe.com/en_US/AIR/1.5/devappsflex/">Developing Adobe AIR Applications with Flex</a>."</p>
		 * 
		 * @param password	The password to use to generate the encryption key.
		 * @param overrideSaltELSKey	The EncryptionKeyGenerator creates and stores a random value 
		 * 								(known as a <i>salt</i>) as part of the process of 
		 * 								generating the encryption key. The first time an application 
		 * 								calls the <code>getEncryptionKey()</code> method, the salt 
		 * 								value is created and stored in the AIR application's encrypted 
		 * 								local store (ELS). From then on, the salt value is loaded from the 
		 * 								ELS.
		 * 								<p>If you wish to provide a custom String ELS key for storing 
		 * 								the salt value, specify a value for the <code>overrideSaltELSKey</code>
		 * 								parameter. If the parameter is <code>null</code> (the default) 
		 * 								a default key name is used.</p>
		 * 
		 * @return The generated encryption key, a 16-byte ByteArray object.
		 * 
		 * @throws ArgumentError	If the specified password is not a "strong" password according to the 
		 * 							criteria explained in the <code>validateStrongPassword()</code> 
		 * 							method description
		 * 
		 * @throws ArgumentError	If a non-<code>null</code> value is specified for the <code>overrideSaltELSKey</code>
		 * 							parameter, and the value is an empty String (<code>""</code>)
		 */
		public function getEncryptionKey(password:String, overrideSaltELSKey:String=null):ByteArray
		{
			if (!validateStrongPassword(password))
			{
				throw new ArgumentError("The password must be a strong password. It must be 8-32 characters long. It must contain at least one uppercase letter, at least one lowercase letter, and at least one number or symbol.");
			}
			
			if (overrideSaltELSKey != null && overrideSaltELSKey.length <= 0)
			{
				throw new ArgumentError("If an overrideSaltELSKey parameter value is specified, it can't be an empty String.");
			}
			
			var concatenatedPassword:String = concatenatePassword(password);
			
			var saltKey:String;
			if (overrideSaltELSKey == null)
			{
				saltKey = SALT_ELS_KEY;
			}
			else
			{
				saltKey = overrideSaltELSKey;
			}
			
			var salt:ByteArray = EncryptedLocalStore.getItem(saltKey);
			if (salt == null)
			{
				salt = makeSalt();
				EncryptedLocalStore.setItem(saltKey, salt);
			}
			
			var unhashedKey:ByteArray = xorBytes(concatenatedPassword, salt);
			
			var hashedKey:String = SHA256.hashBytes(unhashedKey);
			
			var encryptionKey:ByteArray = generateEncryptionKey(hashedKey);
			
			return encryptionKey;
		}
		
		
		// ------- Creating encryption key -------
		
		private function concatenatePassword(pwd:String):String
		{
			var len:int = pwd.length;
			var targetLength:int = 32;
			
			if (len == targetLength)
			{
				return pwd;
			}
			
			var repetitions:int = Math.floor(targetLength / len);
			var excess:int = targetLength % len;
			
			var result:String = "";
			
			for (var i:uint = 0; i < repetitions; i++)
			{
				result += pwd;
			}
			
			result += pwd.substr(0, excess);
			
			return result;
		}
		
		
		private function makeSalt():ByteArray
		{
			var result:ByteArray = new ByteArray;
			
			for (var i:uint = 0; i < 8; i++)
			{
				result.writeUnsignedInt(Math.round(Math.random() * uint.MAX_VALUE));
			}
			
			return result;
		}
		
		
		private function xorBytes(passwordString:String, salt:ByteArray):ByteArray
		{
			var result:ByteArray = new ByteArray();
			
			for (var i:uint = 0; i < 32; i += 4)
			{
				// Extract 4 bytes from the password string and convert to a uint
				var o1:uint = passwordString.charCodeAt(i) << 24;
				o1 += passwordString.charCodeAt(i + 1) << 16;
				o1 += passwordString.charCodeAt(i + 2) << 8;
				o1 += passwordString.charCodeAt(i + 3);
				
				salt.position = i;
				var o2:uint = salt.readUnsignedInt();
				
				var xor:uint = o1 ^ o2;
				result.writeUnsignedInt(xor);
			}
			
			return result;
		}
		
		
		private function generateEncryptionKey(hash:String):ByteArray
		{
			var result:ByteArray = new ByteArray();
			
			// select a range of 128 bits (32 hex characters) from the hash
			// In this case, we'll use the bits starting from position 17
			for (var i:uint = 0; i < 32; i += 2)
			{
				var position:uint = i + 17;
				var hex:String = hash.substr(position, 2);
				var byte:int = parseInt(hex, 16);
				result.writeByte(byte);
			}
			
			return result;
		}
	}
}