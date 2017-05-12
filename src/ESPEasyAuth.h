/*
 * ESP EasyAuth
 * Zhenyu Wu (Adam_5Wu@hotmail.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.	If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ESPEasyAuth_H
#define ESPEasyAuth_H

#define ESPEA_LOG(...) Serial.printf(__VA_ARGS__)
#define ESPEA_DEBUG_LEVEL 3

#if ESPEA_DEBUG_LEVEL < 1
	#define ESPEA_DEBUGDO(...)
	#define ESPEA_DEBUG(...)
#else
	#define ESPEA_DEBUGDO(...) __VA_ARGS__
	#define ESPEA_DEBUG(...) Serial.printf(__VA_ARGS__)
#endif

#if ESPEA_DEBUG_LEVEL < 2
	#define ESPEA_DEBUGVDO(...)
	#define ESPEA_DEBUGV(...)
#else
	#define ESPEA_DEBUGVDO(...) __VA_ARGS__
	#define ESPEA_DEBUGV(...) Serial.printf(__VA_ARGS__)
#endif

#if ESPEA_DEBUG_LEVEL < 3
	#define ESPEA_DEBUGVVDO(...)
	#define ESPEA_DEBUGVV(...)
#else
	#define ESPEA_DEBUGVVDO(...) __VA_ARGS__
	#define ESPEA_DEBUGVV(...) Serial.printf(__VA_ARGS__)
#endif

//#define SECURE_SECRET_WIPE
//#define STRICT_PROTOCOL

#include <utility>
#include "WString.h"
#include "LinkedList.h"
#include "StringArray.h"
	
class IdentityProvider;

// Instance only comes from IdentityProvider, and is globally unique
class Identity {
	friend class IdentityProvider;

	protected:
		Identity(String const& id) : ID(id) {}
#ifdef __GXX_EXPERIMENTAL_CXX0X__
		Identity(String && id) : ID(std::move(id)) {}
#endif

		Identity(Identity const&) = delete;
		Identity& operator=(Identity const&) = delete;

	public:
		String const ID;

		String toString(void) const { return ID; }
};

inline bool operator==(const Identity& lhs, const Identity& rhs)
{ return &lhs == &rhs; }

inline bool operator!=(const Identity& lhs, const Identity& rhs)
{ return &lhs != &rhs; }
	
typedef enum {
	EA_SECRET_NONE,
	EA_SECRET_PLAINTEXT,
	EA_SECRET_HTTPDIGESTAUTH_MD5,
	EA_SECRET_HTTPDIGESTAUTH_MD5SESS,
} SecretKind;

struct Credential {
	Identity &IDENT;
	SecretKind SECKIND;
	String SECRET;

	Credential(Identity &ident) : IDENT(ident), SECKIND(EA_SECRET_NONE) {}
	Credential(Identity &ident, SecretKind seckind, String && secret)
	: IDENT(ident), SECKIND(seckind), SECRET(std::move(secret)) {}

	void setSecret(SecretKind seckind, String && secret) {
		disposeSecret();
		SECKIND = seckind;
		SECRET = std::move(secret);
	}

	void disposeSecret(void) {
		if (SECKIND != EA_SECRET_NONE) {
			SECKIND = EA_SECRET_NONE;
#ifdef SECURE_SECRET_WIPE
			memset(SECRET.begin(), SECRET.length(), 0);
#endif
			SECRET.clear(true);
		}
	}
};

class Authorizer {
	public:
		virtual bool Authenticate(Credential& cred) = 0;
		virtual bool Authorize(Identity &ident, Credential& cred) = 0;
};

class BasicAuthorizer : public Authorizer {
	public:
		virtual bool Authorize(Identity &ident, Credential& cred) override
		{ return Authenticate(cred) && (cred.IDENT == ident); }
};

class DummyAuthorizer : public BasicAuthorizer {
	public:
		bool const AuthState;
		
		DummyAuthorizer(bool state = false) : AuthState(state) {}
		
		virtual bool Authenticate(Credential& cred) override
		{ return cred.disposeSecret(), AuthState; }
};

class AuthSession {
	protected:
		Authorizer *AUTH;

	public:
		Identity &IDENT;
		StringArray DATA;

		AuthSession(Identity &ident, Authorizer *auth)
		: AUTH(auth), IDENT(ident) {}

		AuthSession(Credential &cred, Authorizer *auth)
		: AUTH(auth->Authenticate(cred)?NULL:auth), IDENT(cred.IDENT) {}

		AuthSession(AuthSession &&r)
		: AUTH(r.AUTH), IDENT(r.IDENT), DATA(std::move(DATA)) {}
	
		bool isAuthorized(void) const { return !AUTH; }

		bool Authorize(SecretKind skind, char const *secret)
		{ return Authorize(skind, String(secret)); }
		bool Authorize(SecretKind skind, String && secret) {
			Credential C(IDENT, skind, std::move(secret));
			return Authorize(C);
		}
		bool Authorize(Credential &cred) {
			if (AUTH && AUTH->Authorize(IDENT, cred)) AUTH = NULL;
			return isAuthorized();
		}

		String toString(void) const {
			String Ret;
			Ret.concat('{');
			Ret.concat(IDENT.toString());
			Ret.concat('(');
			Ret.concat(isAuthorized()?"Authorized":"Unauthorized");
			Ret.concat(")}",2);
			return Ret;
		}
};

class IdentityProvider {
	protected:
		Identity* CreateIdentity(String const& id) { return new Identity(id); }
	public:
		static Identity UNKNOWN_IDENTITY;
		static Identity ANONYMOUS;
		virtual Identity& getIdentity(String const& identName) const = 0;

		LinkedList<Identity*> parseIdentities(char const *Str) const;
		String mapIdentities(LinkedList<Identity*> const &idents) const;
};

class DummyIdentityProvider : public IdentityProvider {
	public:
		virtual Identity& getIdentity(String const& identName) const override
		{ return UNKNOWN_IDENTITY; }
};

class SessionAuthority {
	public:
		IdentityProvider * const IDP;
		Authorizer * const AUTH;

		SessionAuthority(IdentityProvider *idp, Authorizer *auth)
		: IDP(idp), AUTH(auth) {}

		AuthSession getSession(String const& identName)
		{ return AuthSession(IDP->getIdentity(identName), AUTH); }

		AuthSession getSession(char const* identName, SecretKind skind, char const* secret)
		{ return getSession(identName, skind, String(secret)); }
		AuthSession getSession(String const& identName, SecretKind skind, String && secret)
		{ return getSession(Credential(IDP->getIdentity(identName), skind, std::move(secret))); }
		AuthSession getSession(Credential &&cred)
		{ return AuthSession(cred, AUTH); }
};

class DummySessionAuthority : public SessionAuthority {
	protected:
		DummyIdentityProvider D_IDP;
		DummyAuthorizer D_AUTH;

	public:
		DummySessionAuthority(bool authState = false)
		: SessionAuthority(&D_IDP, &D_AUTH), D_AUTH(authState) {}
};

class SimpleAccountAuthority : public IdentityProvider, public BasicAuthorizer {
	protected:
		bool _AllowNoPassword;
		struct SimpleAccount {
			Identity* IDENT;
			String Password;
		};
		LinkedList<SimpleAccount> Accounts;

	public:
		SimpleAccountAuthority(bool AllowNoPassword = true)
		: _AllowNoPassword(AllowNoPassword), Accounts([](SimpleAccount &x){delete x.IDENT;}) {}
		~SimpleAccountAuthority(void) {}

		size_t addAccount(char const *identName, char const *password);
		bool removeAccount(char const *identName);

		size_t loadAccounts(Stream &source);

		virtual Identity& getIdentity(String const& identName) const override;
		virtual bool Authenticate(Credential& cred) override;
};

#endif // ESPEasyAuth_H
