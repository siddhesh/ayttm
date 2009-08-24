/*
 * Ayttm
 *
 * Copyright (C) 2009, the Ayttm team
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

const char *MSN_SOAP_LOGIN_REQUEST = 
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
"<Envelope xmlns=\"http://schemas.xmlsoap.org/soap/envelope/\" "
	"xmlns:wsse=\"http://schemas.xmlsoap.org/ws/2003/06/secext\" "
	"xmlns:saml=\"urn:oasis:names:tc:SAML:1.0:assertion\" "
	"xmlns:wsp=\"http://schemas.xmlsoap.org/ws/2002/12/policy\" "
	"xmlns:wsu=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd\" "
	"xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/03/addressing\" "
	"xmlns:wssc=\"http://schemas.xmlsoap.org/ws/2004/04/sc\" "
	"xmlns:wst=\"http://schemas.xmlsoap.org/ws/2004/04/trust\">"
	"<Header>"
		"<ps:AuthInfo "
			"xmlns:ps=\"http://schemas.microsoft.com/Passport/SoapServices/PPCRL\" "
			"Id=\"PPAuthInfo\">"
			"<ps:HostingApp>{7108E71A-9926-4FCB-BCC9-9A9D3F32E423}</ps:HostingApp>"
			"<ps:BinaryVersion>4</ps:BinaryVersion>"
			"<ps:UIVersion>1</ps:UIVersion>"
			"<ps:Cookies></ps:Cookies>"
			"<ps:RequestParams>AQAAAAIAAABsYwQAAAAxMDMz</ps:RequestParams>"
		"</ps:AuthInfo>"
		/* Our Auth credentials */
		"<wsse:Security>"
			"<wsse:UsernameToken Id=\"user\">"
				"<wsse:Username>%s</wsse:Username>"
				"<wsse:Password>%s</wsse:Password>"
			"</wsse:UsernameToken>"
		"</wsse:Security>"
	"</Header>"
	"<Body>"
		"<ps:RequestMultipleSecurityTokens "
			"xmlns:ps=\"http://schemas.microsoft.com/Passport/SoapServices/PPCRL\" "
			"Id=\"RSTS\">"
			"<wst:RequestSecurityToken Id=\"RST0\">"
				"<wst:RequestType>http://schemas.xmlsoap.org/ws/2004/04/security/trust/Issue</wst:RequestType>"
				"<wsp:AppliesTo>"
					"<wsa:EndpointReference>"
						"<wsa:Address>http://Passport.NET/tb</wsa:Address>"
					"</wsa:EndpointReference>"
				"</wsp:AppliesTo>"
			"</wst:RequestSecurityToken>"
			/* Auth block */
			"<wst:RequestSecurityToken Id=\"RST1\">"
				"<wst:RequestType>http://schemas.xmlsoap.org/ws/2004/04/security/trust/Issue</wst:RequestType>"
				"<wsp:AppliesTo>"
					"<wsa:EndpointReference>"
						"<wsa:Address>messengerclear.live.com</wsa:Address>"
					"</wsa:EndpointReference>"
				"</wsp:AppliesTo>"
				"<wsse:PolicyReference URI=\"%s\"></wsse:PolicyReference>"
			"</wst:RequestSecurityToken>"
			/* Contacts Block */
/**/			"<wst:RequestSecurityToken Id=\"RST2\">"
				"<wst:RequestType>http://schemas.xmlsoap.org/ws/2004/04/security/trust/Issue</wst:RequestType>"
				"<wsp:AppliesTo>"
					"<wsa:EndpointReference>"
						"<wsa:Address>contacts.msn.com</wsa:Address>"
					"</wsa:EndpointReference>"
				"</wsp:AppliesTo>"
				"<wsse:PolicyReference URI=\"MBI\"></wsse:PolicyReference>"
				//"<wsse:PolicyReference URI=\"?fs=1&id=24000&kv=9&rn=93S9SWWw&tw=0&ver=2.1.6000.1\"></wsse:PolicyReference>"
			"</wst:RequestSecurityToken>"/**/
		"</ps:RequestMultipleSecurityTokens>"
	"</Body>"
"</Envelope>";

const char *MSN_MEMBERSHIP_LIST_REQUEST = 
"<?xml version='1.0' encoding='utf-8'?>"
"<soap:Envelope xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\">"
	"<soap:Header xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\">"
		"<ABApplicationHeader xmlns=\"http://www.msn.com/webservices/AddressBook\">"
			"<ApplicationId xmlns=\"http://www.msn.com/webservices/AddressBook\">CFE80F9D-180F-4399-82AB-413F33A1FA11</ApplicationId>"
			"<IsMigration xmlns=\"http://www.msn.com/webservices/AddressBook\">false</IsMigration>"
			"<PartnerScenario xmlns=\"http://www.msn.com/webservices/AddressBook\">Initial</PartnerScenario>"
		"</ABApplicationHeader>"
		"<ABAuthHeader xmlns=\"http://www.msn.com/webservices/AddressBook\">"
			"<ManagedGroupRequest xmlns=\"http://www.msn.com/webservices/AddressBook\">false</ManagedGroupRequest>"
			"<TicketToken>%s</TicketToken>"
		"</ABAuthHeader>"
	"</soap:Header>"
	"<soap:Body xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\">"
		"<FindMembership xmlns=\"http://www.msn.com/webservices/AddressBook\">"
			"<serviceFilter xmlns=\"http://www.msn.com/webservices/AddressBook\">"
					"<Types xmlns=\"http://www.msn.com/webservices/AddressBook\">"
						"<ServiceType xmlns=\"http://www.msn.com/webservices/AddressBook\">Messenger</ServiceType>"
						"<ServiceType xmlns=\"http://www.msn.com/webservices/AddressBook\">Invitation</ServiceType>"
						"<ServiceType xmlns=\"http://www.msn.com/webservices/AddressBook\">Space</ServiceType>"
						"<ServiceType xmlns=\"http://www.msn.com/webservices/AddressBook\">Profile</ServiceType>"
					"</Types>"
			"</serviceFilter>"
		"</FindMembership>"
	"</soap:Body>"
"</soap:Envelope>";


const char *MSN_CONTACT_LIST_REQUEST = 
"<?xml version=\"1.0\" encoding=\"utf-8\"?>"
"<soap:Envelope xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" xmlns:soapenc=\"http://schemas.xmlsoap.org/soap/encoding/\">"
	"<soap:Header>"
		"<ABApplicationHeader xmlns=\"http://www.msn.com/webservices/AddressBook\">"
			"<ApplicationId>CFE80F9D-180F-4399-82AB-413F33A1FA11</ApplicationId>"
			"<IsMigration>false</IsMigration>"
			"<PartnerScenario>Initial</PartnerScenario>"
		"</ABApplicationHeader>"
		"<ABAuthHeader xmlns=\"http://www.msn.com/webservices/AddressBook\">"
			"<ManagedGroupRequest>false</ManagedGroupRequest>"
			"<TicketToken>%s</TicketToken>"
		"</ABAuthHeader>"
	"</soap:Header>"
	"<soap:Body>"
		"<ABFindAll xmlns=\"http://www.msn.com/webservices/AddressBook\">"
			"<abId>00000000-0000-0000-0000-000000000000</abId>"
			"<abView>Full</abView>"
			"<deltasOnly>false</deltasOnly>"
			"<lastChange>0001-01-01T00:00:00.0000000-08:00</lastChange>"
		"</ABFindAll>"
	"</soap:Body>"
"</soap:Envelope>";

const char *MSN_MEMBERSHIP_LIST_MODIFY = 
"<?xml version=\"1.0\" encoding=\"utf-8\"?>"
"<soap:Envelope xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\" "
		"xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
		"xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" "
		"xmlns:soapenc=\"http://schemas.xmlsoap.org/soap/encoding/\">"
	"<soap:Header>"
		"<ABApplicationHeader xmlns=\"http://www.msn.com/webservices/AddressBook\">"
			"<ApplicationId>CFE80F9D-180F-4399-82AB-413F33A1FA11</ApplicationId>"
			"<IsMigration>false</IsMigration>"
			"<PartnerScenario>%s</PartnerScenario>"
		"</ABApplicationHeader>"
		"<ABAuthHeader xmlns=\"http://www.msn.com/webservices/AddressBook\">"
			"<ManagedGroupRequest>false</ManagedGroupRequest>"
			"<TicketToken>%s</TicketToken>"
		"</ABAuthHeader>"
	"</soap:Header>"
	"<soap:Body>"
		"<%s xmlns=\"http://www.msn.com/webservices/AddressBook\">"
			"<serviceHandle>"
				"<Id>0</Id>"
				"<Type>Messenger</Type>"
				"<ForeignId></ForeignId>"
			"</serviceHandle>"
			"<memberships>"
				"<Membership>"
					"<MemberRole>%s</MemberRole>"
					"<Members>"
						"%s"	/* Member tag */
					"</Members>"
				"</Membership>"
			"</memberships>"
		"</%s>"
	"</soap:Body>"
"</soap:Envelope>";


const char *MSN_CONTACT_ADD_REQUEST = 
"<soap:Envelope "
		"xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\" "
		"xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
		"xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" "
		"xmlns:soapenc=\"http://schemas.xmlsoap.org/soap/encoding/\">"
	"<soap:Header>"
		"<ABApplicationHeader xmlns=\"http://www.msn.com/webservices/AddressBook\">"
			"<ApplicationId>CFE80F9D-180F-4399-82AB-413F33A1FA11</ApplicationId>"
			"<IsMigration>false</IsMigration>"
			"<PartnerScenario>%s</PartnerScenario>"
		"</ABApplicationHeader>"
		"<ABAuthHeader xmlns=\"http://www.msn.com/webservices/AddressBook\">"
			"<ManagedGroupRequest>false</ManagedGroupRequest>"
			"<TicketToken>%s</TicketToken>"
		"</ABAuthHeader>"
	"</soap:Header>"
	"<soap:Body>"
		"<ABContactAdd xmlns=\"http://www.msn.com/webservices/AddressBook\">"
			"<abId>00000000-0000-0000-0000-000000000000</abId>"
			"<contacts>"
				"<Contact xmlns=\"http://www.msn.com/webservices/AddressBook\">"
					"<contactInfo>"
						"%s"	/* Either Passport or Email */
					"</contactInfo>"
				"</Contact>"
			"</contacts>"
			"<options>"
				"<EnableAllowListManagement>true</EnableAllowListManagement>"
			"</options>"
		"</ABContactAdd>"
	"</soap:Body>"
"</soap:Envelope>";



const char *MSN_CONTACT_UPDATE_REQUEST = 
"<soap:Envelope "
		"xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\" "
		"xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
		"xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" "
		"xmlns:soapenc=\"http://schemas.xmlsoap.org/soap/encoding/\">"
	"<soap:Header>"
		"<ABApplicationHeader xmlns=\"http://www.msn.com/webservices/AddressBook\">"
				"<ApplicationId>CFE80F9D-180F-4399-82AB-413F33A1FA11</ApplicationId>"
				"<IsMigration>false</IsMigration>"
				"<PartnerScenario>%s</PartnerScenario>"
		"</ABApplicationHeader>"
		"<ABAuthHeader xmlns=\"http://www.msn.com/webservices/AddressBook\">"
				"<ManagedGroupRequest>false</ManagedGroupRequest>"
				"<TicketToken>%s</TicketToken>"
		"</ABAuthHeader>"
	"</soap:Header>"
	"<soap:Body>"
		"<ABContactUpdate xmlns=\"http://www.msn.com/webservices/AddressBook\">"
			"<abId>00000000-0000-0000-0000-000000000000</abId>"
			"<contacts>"
				"<Contact xmlns=\"http://www.msn.com/webservices/AddressBook\">"
					"<contactId>%s</contactId>"
					"<contactInfo>"
						"<displayName/>"
						"<passportName>%s</passportName>"
						"<isMessengerUser>false</isMessengerUser>"
					"</contactInfo>"
					"<propertiesChanged>DisplayName IsMessengerUser</propertiesChanged>"
				"</Contact>"
			"</contacts>"
		"</ABContactUpdate>"
	"</soap:Body>"
"</soap:Envelope>";


const char *MSN_CONTACT_DELETE_REQUEST = 
"<soap:Envelope xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\" "
"xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
"xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" "
"xmlns:soapenc=\"http://schemas.xmlsoap.org/soap/encoding/\">"
	"<soap:Header>"
		"<ABApplicationHeader xmlns=\"http://www.msn.com/webservices/AddressBook\">"
			"<ApplicationId>CFE80F9D-180F-4399-82AB-413F33A1FA11</ApplicationId>"
			"<IsMigration>false</IsMigration>"
			"<PartnerScenario>Timer</PartnerScenario>"
		"</ABApplicationHeader>"
		"<ABAuthHeader xmlns=\"http://www.msn.com/webservices/AddressBook\">"
			"<ManagedGroupRequest>false</ManagedGroupRequest>"
			"<TicketToken>%s</TicketToken>"
		"</ABAuthHeader>"
	"</soap:Header>"
	"<soap:Body>"
		"<ABContactDelete xmlns=\"http://www.msn.com/webservices/AddressBook\">"
			"<abId>00000000-0000-0000-0000-000000000000</abId>"
			"<contacts>"
				"<Contact>"
					"<contactId>%s</contactId>"
				"</Contact>"
			"</contacts>"
		"</ABContactDelete>"
	"</soap:Body>"
"</soap:Envelope>";


const char *MSN_GROUP_ADD_REQUEST =
"<soap:Envelope "
"xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\" "
"xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
"xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" "
"xmlns:soapenc=\"http://schemas.xmlsoap.org/soap/encoding/\">"
	"<soap:Header>"
		"<ABApplicationHeader xmlns=\"http://www.msn.com/webservices/AddressBook\">"
			"<ApplicationId>CFE80F9D-180F-4399-82AB-413F33A1FA11</ApplicationId>"
			"<IsMigration>false</IsMigration>"
			"<PartnerScenario>GroupSave</PartnerScenario>"
		"</ABApplicationHeader>"
		"<ABAuthHeader xmlns=\"http://www.msn.com/webservices/AddressBook\">"
			"<ManagedGroupRequest>false</ManagedGroupRequest>"
			"<TicketToken>%s</TicketToken>"
		"</ABAuthHeader>"
	"</soap:Header>"
	"<soap:Body>"
		"<ABGroupAdd xmlns=\"http://www.msn.com/webservices/AddressBook\">"
			"<abId>00000000-0000-0000-0000-000000000000</abId>"
			"<groupAddOptions>"
				"<fRenameOnMsgrConflict>false</fRenameOnMsgrConflict>"
			"</groupAddOptions>"
			"<groupInfo>"
				"<GroupInfo>"
					"<name>%s</name>"
					"<groupType>C8529CE2-6EAD-434d-881F-341E17DB3FF8</groupType>"
					"<fMessenger>false</fMessenger>"
					"<annotations>"
						"<Annotation>"
							"<Name>MSN.IM.Display</Name>"
							"<Value>1</Value>"
						"</Annotation>"
					"</annotations>"
				"</GroupInfo>"
			"</groupInfo>"
		"</ABGroupAdd>"
	"</soap:Body>"
"</soap:Envelope>";


const char *MSN_GROUP_MOD_REQUEST = 
"<soap:Envelope "
"xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\" "
"xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
"xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" "
"xmlns:soapenc=\"http://schemas.xmlsoap.org/soap/encoding/\">"
	"<soap:Header>"
		"<ABApplicationHeader xmlns=\"http://www.msn.com/webservices/AddressBook\">"
			"<ApplicationId>CFE80F9D-180F-4399-82AB-413F33A1FA11</ApplicationId>"
			"<IsMigration>false</IsMigration>"
			"<PartnerScenario>GroupSave</PartnerScenario>"
		"</ABApplicationHeader>"
		"<ABAuthHeader xmlns=\"http://www.msn.com/webservices/AddressBook\">"
			"<ManagedGroupRequest>false</ManagedGroupRequest>"
			"<TicketToken>%s</TicketToken>"
		"</ABAuthHeader>"
	"</soap:Header>"
	"<soap:Body>"
		"<ABGroupUpdate xmlns=\"http://www.msn.com/webservices/AddressBook\">"
			"<abId>00000000-0000-0000-0000-000000000000</abId>"
			"<groups>"
				"<Group>"
					"<groupId>%s</groupId>"
					"<groupInfo>"
						"<name>%s</name>"
					"</groupInfo>"
					"<propertiesChanged>GroupName</propertiesChanged>"
				"</Group>"
			"</groups>"
		"</ABGroupUpdate>"
	"</soap:Body>"
"</soap:Envelope>";


const char *MSN_GROUP_DEL_REQUEST =
"<soap:Envelope "
"xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\" "
"xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
"xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" "
"xmlns:soapenc=\"http://schemas.xmlsoap.org/soap/encoding/\">"
	"<soap:Header>"
		"<ABApplicationHeader xmlns=\"http://www.msn.com/webservices/AddressBook\">"
			"<ApplicationId>CFE80F9D-180F-4399-82AB-413F33A1FA11</ApplicationId>"
			"<IsMigration>false</IsMigration>"
			"<PartnerScenario>Timer</PartnerScenario>"
		"</ABApplicationHeader>"
		"<ABAuthHeader xmlns=\"http://www.msn.com/webservices/AddressBook\">"
			"<ManagedGroupRequest>false</ManagedGroupRequest>"
			"<TicketToken>%s</TicketToken>"
		"</ABAuthHeader>"
	"</soap:Header>"
	"<soap:Body>"
		"<ABGroupDelete xmlns=\"http://www.msn.com/webservices/AddressBook\">"
			"<abId>00000000-0000-0000-0000-000000000000</abId>"
			"<groupFilter>"
				"<groupIds>"
					"<guid>%s</guid>"
				"</groupIds>"
			"</groupFilter>"
		"</ABGroupDelete>"
	"</soap:Body>"
"</soap:Envelope>";


const char *MSN_GROUP_CONTACT_REQUEST = 
"<soap:Envelope "
"xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\" "
"xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
"xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" "
"xmlns:soapenc=\"http://schemas.xmlsoap.org/soap/encoding/\">"
	"<soap:Header>"
		"<ABApplicationHeader xmlns=\"http://www.msn.com/webservices/AddressBook\">"
			"<ApplicationId>CFE80F9D-180F-4399-82AB-413F33A1FA11</ApplicationId>"
			"<IsMigration>false</IsMigration>"
			"<PartnerScenario>GroupSave</PartnerScenario>"
		"</ABApplicationHeader>"
		"<ABAuthHeader xmlns=\"http://www.msn.com/webservices/AddressBook\">"
			"<ManagedGroupRequest>false</ManagedGroupRequest>"
			"<TicketToken>%s</TicketToken>"
		"</ABAuthHeader>"
	"</soap:Header>"
	"<soap:Body>"
		"<ABGroupContact%s xmlns=\"http://www.msn.com/webservices/AddressBook\">"
			"<abId>00000000-0000-0000-0000-000000000000</abId>"
			"<groupFilter>"
				"<groupIds>"
					"<guid>%s</guid>"
				"</groupIds>"
			"</groupFilter>"
			"<contacts>"
				"<Contact>"
					"<contactId>%s</contactId>"
				"</Contact>"
			"</contacts>"
		"</ABGroupContact%s>"
	"</soap:Body>"
"</soap:Envelope>";


char *msn_soap_build_request(const char *skel, ...)
{
	char buf[4096];
	va_list ap;

	va_start(ap, skel);

	vsnprintf(buf, sizeof(buf), skel, ap);

	va_end(ap);

	return strdup(buf);
}


