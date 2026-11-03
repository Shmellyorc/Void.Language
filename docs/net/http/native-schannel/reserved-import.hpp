// Isolated experiment implementing Microsoft's documented persisted-key import.
// Included after the existing probe helpers; never linked into production.
struct EphemeralKey {
  NCRYPT_KEY_HANDLE key=0;
  ~EphemeralKey(){if(key)NCryptFreeObject(key);}
};

static const wchar_t* private_blob_type(NCRYPT_KEY_HANDLE key){
  auto group=property(key,NCRYPT_ALGORITHM_GROUP_PROPERTY);
  auto value=reinterpret_cast<const wchar_t*>(group.data());
  if(wcscmp(value,NCRYPT_RSA_ALGORITHM_GROUP)==0)return BCRYPT_RSAPRIVATE_BLOB;
  // Windows PFX decoding can expose an ECDSA certificate through an ECDH-group
  // CNG key. Both groups use the documented ECC private blob format.
  if(wcscmp(value,NCRYPT_ECDSA_ALGORITHM_GROUP)==0||wcscmp(value,NCRYPT_ECDH_ALGORITHM_GROUP)==0)return BCRYPT_ECCPRIVATE_BLOB;
  throw std::runtime_error("unsupported private blob algorithm");
}

static void prepare_raw_import(NCRYPT_PROV_HANDLE provider,Secret& encrypted,
                               NCryptBuffer& password,NCRYPT_KEY_HANDLE original,
                               Secret& raw){
  // This unnamed key is intentionally exportable only within this experiment.
  // No key name, machine flag, legacy store or persistence fallback is supplied.
  EphemeralKey bridge_key;NCryptBufferDesc parameters{NCRYPTBUFFER_VERSION,1,&password};
  require("bridge_ephemeral_import",NCryptImportKey(provider,0,NCRYPT_PKCS8_PRIVATE_KEY_BLOB,&parameters,
    &bridge_key.key,encrypted.v.data(),static_cast<DWORD>(encrypted.v.size()),NCRYPT_DO_NOT_FINALIZE_FLAG));
  DWORD export_policy=NCRYPT_ALLOW_EXPORT_FLAG|NCRYPT_ALLOW_PLAINTEXT_EXPORT_FLAG;
  require("bridge_temporary_export_policy",NCryptSetProperty(bridge_key.key,NCRYPT_EXPORT_POLICY_PROPERTY,
    reinterpret_cast<BYTE*>(&export_policy),sizeof export_policy,0));
  require("bridge_ephemeral_finalize",NCryptFinalizeKey(bridge_key.key,0));
  if(public_blob(bridge_key.key)!=public_blob(original))throw std::runtime_error("bridge public mismatch");
  const auto type=private_blob_type(original);DWORD n=0;
  require("bridge_raw_size",NCryptExportKey(bridge_key.key,0,type,nullptr,nullptr,0,&n,0));
  if(!n||n>1024*1024)throw std::runtime_error("raw blob bound");raw.v.resize(n);
  require("bridge_raw_export",NCryptExportKey(bridge_key.key,0,type,nullptr,raw.v.data(),n,&n,0));raw.v.resize(n);
  printf("{\"temporary_raw_blob_bytes\":%lu,\"temporary_bridge_export_policy\":3}\n",n);
}

static SECURITY_STATUS create_import_handle(NCRYPT_PROV_HANDLE provider,NCRYPT_KEY_HANDLE original,
                                            const std::wstring& name,Secret& raw,
                                            NCRYPT_KEY_HANDLE* destination){
  auto algorithm=property(original,NCRYPT_ALGORITHM_PROPERTY);
  SECURITY_STATUS s=NCryptCreatePersistedKey(provider,destination,
    reinterpret_cast<const wchar_t*>(algorithm.data()),name.c_str(),0,0);
  result("candidate_create_no_overwrite",s);if(s)return s;
  s=NCryptSetProperty(*destination,private_blob_type(original),raw.v.data(),static_cast<DWORD>(raw.v.size()),0);
  result("candidate_set_import_blob",s);return s;
}

static void pending_policy(NCRYPT_KEY_HANDLE key,const std::vector<BYTE>& acl,const BYTE* marker){
  DWORD policy=0,usage=NCRYPT_ALLOW_SIGNING_FLAG;
  require("candidate_pending_export_zero",NCryptSetProperty(key,NCRYPT_EXPORT_POLICY_PROPERTY,reinterpret_cast<BYTE*>(&policy),sizeof policy,0));
  require("candidate_pending_sign_only",NCryptSetProperty(key,NCRYPT_KEY_USAGE_PROPERTY,reinterpret_cast<BYTE*>(&usage),sizeof usage,0));
  require("candidate_pending_dacl",NCryptSetProperty(key,NCRYPT_SECURITY_DESCR_PROPERTY,const_cast<BYTE*>(acl.data()),static_cast<DWORD>(acl.size()),DACL_SECURITY_INFORMATION));
  require("candidate_pending_marker",NCryptSetProperty(key,L"VOID-SCHANNEL-TEST-OWNER",const_cast<BYTE*>(marker),32,NCRYPT_PERSIST_FLAG|NCRYPT_PERSIST_ONLY_FLAG));
}

struct KeySnapshot {
  std::vector<std::vector<BYTE>> fields;
  SECURITY_STATUS marker_status=0,plain=0,pkcs8=0;
  bool operator==(const KeySnapshot& b)const{return fields==b.fields&&marker_status==b.marker_status&&plain==b.plain&&pkcs8==b.pkcs8;}
};
static KeySnapshot snapshot(NCRYPT_KEY_HANDLE key,const char* label){
  KeySnapshot out;out.fields.push_back(public_blob(key));
  BYTE marker[32]{};DWORD n=sizeof marker;
  out.marker_status=NCryptGetProperty(key,L"VOID-SCHANNEL-TEST-OWNER",marker,sizeof marker,&n,NCRYPT_PERSIST_ONLY_FLAG);
  out.fields.push_back(out.marker_status?std::vector<BYTE>{}:std::vector<BYTE>(marker,marker+n));
  for(auto name:{NCRYPT_NAME_PROPERTY,NCRYPT_UNIQUE_NAME_PROPERTY,NCRYPT_ALGORITHM_PROPERTY,NCRYPT_LENGTH_PROPERTY,
                NCRYPT_KEY_TYPE_PROPERTY,NCRYPT_EXPORT_POLICY_PROPERTY,NCRYPT_KEY_USAGE_PROPERTY})out.fields.push_back(property(key,name));
  out.fields.push_back(property(key,NCRYPT_SECURITY_DESCR_PROPERTY,DACL_SECURITY_INFORMATION));
  out.plain=NCryptExportKey(key,0,private_blob_type(key),nullptr,nullptr,0,&n,0);
  out.pkcs8=NCryptExportKey(key,0,NCRYPT_PKCS8_PRIVATE_KEY_BLOB,nullptr,nullptr,0,&n,0);
  std::vector<std::string> hashes;for(size_t i=2;i<out.fields.size();i++)hashes.push_back(digest(out.fields[i].data(),static_cast<DWORD>(out.fields[i].size())));
  printf("{\"snapshot\":\"%s\",\"public_blob\":\"%s\",\"owner_marker\":\"%s\",\"metadata_sha256\":[",label,
    hex(out.fields[0].data(),out.fields[0].size()).c_str(),hex(out.fields[1].data(),out.fields[1].size()).c_str());
  for(size_t i=0;i<hashes.size();i++)printf("%s\"%s\"",i?",":"",hashes[i].c_str());
  printf("]}\n");result("snapshot_marker",out.marker_status);result("snapshot_raw_private_export",out.plain);result("snapshot_pkcs8_export",out.pkcs8);
  return out;
}
static KeySnapshot reopened_snapshot(NCRYPT_PROV_HANDLE provider,const std::wstring& name,const char* label){
  EphemeralKey check;require("snapshot_reopen",NCryptOpenKey(provider,&check.key,name.c_str(),0,0));return snapshot(check.key,label);
}

static void reserved_collision_gate(Owner& main,NCRYPT_KEY_HANDLE original,Secret& raw,const BYTE* marker,Secret& encrypted,NCryptBuffer& password){
  // First test the same material, then a separately generated different-key sentinel.
  auto before=reopened_snapshot(main.provider,main.name,"same_before");
  EphemeralKey rejected;auto s=create_import_handle(main.provider,original,main.name,raw,&rejected.key);
  if(s!=NTE_EXISTS)throw std::runtime_error("candidate same-key collision accepted");
  if(!(reopened_snapshot(main.provider,main.name,"same_after")==before))throw std::runtime_error("same-key sentinel changed");
  result("candidate_same_key_sentinel_unchanged",0);
  auto algorithm=property(original,NCRYPT_ALGORITHM_PROPERTY);auto acl=private_dacl(user_sid());
  {
    Owner sentinel;sentinel.name=main.name+L"-different";
    require("gate_provider",NCryptOpenStorageProvider(&sentinel.provider,MS_KEY_STORAGE_PROVIDER,0));
    require("gate_different_create",NCryptCreatePersistedKey(sentinel.provider,&sentinel.key,reinterpret_cast<const wchar_t*>(algorithm.data()),sentinel.name.c_str(),0,0));
    pending_policy(sentinel.key,acl,marker);require("gate_different_finalize",NCryptFinalizeKey(sentinel.key,0));sentinel.owned_key=true;
    auto before=reopened_snapshot(sentinel.provider,sentinel.name,"different_before");
    EphemeralKey collision;s=create_import_handle(main.provider,original,sentinel.name,raw,&collision.key);
    if(s!=NTE_EXISTS)throw std::runtime_error("candidate different-key collision accepted");
    if(!(reopened_snapshot(main.provider,sentinel.name,"different_after")==before))throw std::runtime_error("different-key sentinel changed");
    result("candidate_different_key_sentinel_unchanged",0);
  }
  for(bool bad_blob:{true,false}){
    const auto name=main.name+(bad_blob?L"-bad-blob":L"-bad-finalize");
    {
      EphemeralKey failed;require("failure_prepare",NCryptCreatePersistedKey(main.provider,&failed.key,reinterpret_cast<const wchar_t*>(algorithm.data()),name.c_str(),0,0));
      BYTE invalid=0;
      s=bad_blob?NCryptSetProperty(failed.key,private_blob_type(original),&invalid,1,0):NCryptFinalizeKey(failed.key,0x80000000);
      result(bad_blob?"malformed_blob_rejected":"invalid_finalize_flags_rejected",s);
      if(!s)throw std::runtime_error("injected failure accepted");
    }
    EphemeralKey check;s=NCryptOpenKey(main.provider,&check.key,name.c_str(),0,0);result("failure_name_reopen",s);
    if(s!=NTE_BAD_KEYSET)throw std::runtime_error("failure persisted key");
  }
  // Differential controls isolate the named import entry point and blob format.
  // Every target is a separately created synthetic key owned by this invocation.
  for(bool pkcs8:{true,false})for(bool deferred:{false,true}){
    const std::string label=std::string(pkcs8?"pkcs8":"raw")+(deferred?"_deferred":"_immediate");
    Owner sentinel;sentinel.name=main.name+L"-"+widen(label);
    require("matrix_provider",NCryptOpenStorageProvider(&sentinel.provider,MS_KEY_STORAGE_PROVIDER,0));
    require("matrix_create",NCryptCreatePersistedKey(sentinel.provider,&sentinel.key,reinterpret_cast<const wchar_t*>(algorithm.data()),sentinel.name.c_str(),0,0));
    pending_policy(sentinel.key,acl,marker);require("matrix_sentinel_finalize",NCryptFinalizeKey(sentinel.key,0));sentinel.owned_key=true;
    auto before=reopened_snapshot(main.provider,sentinel.name,(label+"_before").c_str());
    NCryptBuffer named{static_cast<ULONG>((sentinel.name.size()+1)*sizeof(wchar_t)),NCRYPTBUFFER_PKCS_KEY_NAME,sentinel.name.data()};
    NCryptBuffer buffers[]{named,password};NCryptBufferDesc parameters{NCRYPTBUFFER_VERSION,pkcs8?2UL:1UL,buffers};EphemeralKey imported;
    auto& bytes=pkcs8?encrypted.v:raw.v;
    s=NCryptImportKey(main.provider,0,pkcs8?NCRYPT_PKCS8_PRIVATE_KEY_BLOB:private_blob_type(original),&parameters,&imported.key,bytes.data(),static_cast<DWORD>(bytes.size()),deferred?NCRYPT_DO_NOT_FINALIZE_FLAG:0);
    printf("{\"matrix_case\":\"%s\"}\n",label.c_str());result("matrix_import",s);
    if(!s&&deferred){s=NCryptFinalizeKey(imported.key,0);result("matrix_finalize",s);}
    auto after=reopened_snapshot(main.provider,sentinel.name,(label+"_after").c_str());
    printf("{\"matrix_result\":\"%s\",\"public_unchanged\":%s,\"all_properties_unchanged\":%s}\n",label.c_str(),before.fields[0]==after.fields[0]?"true":"false",before==after?"true":"false");
  }
  for(unsigned round=0;round<8;round++){
    const auto name=main.name+L"-raw-race-"+std::to_wstring(round);
    const auto blob_type=private_blob_type(original);
    NCRYPT_KEY_HANDLE handles[8]{};SECURITY_STATUS statuses[8]{};
    std::atomic<unsigned> ready{0};std::atomic<bool> go{false};std::vector<std::thread> workers;
    for(unsigned i=0;i<8;i++)workers.emplace_back([&,i]{
      NCryptBuffer named{static_cast<ULONG>((name.size()+1)*sizeof(wchar_t)),NCRYPTBUFFER_PKCS_KEY_NAME,const_cast<wchar_t*>(name.c_str())};
      NCryptBufferDesc parameters{NCRYPTBUFFER_VERSION,1,&named};ready.fetch_add(1);
      while(!go.load())Sleep(0);
      statuses[i]=NCryptImportKey(main.provider,0,blob_type,&parameters,&handles[i],raw.v.data(),static_cast<DWORD>(raw.v.size()),0);
    });
    while(ready.load()!=8)Sleep(0);go.store(true);for(auto& worker:workers)worker.join();
    unsigned successes=0;bool deleted=false;for(unsigned i=0;i<8;i++){
      result("raw_competing_import",statuses[i]);if(!statuses[i])++successes;
      if(handles[i]){
        if(!deleted&&!statuses[i]){require("raw_race_owned_delete",NCryptDeleteKey(handles[i],0));deleted=true;}
        else NCryptFreeObject(handles[i]);
      }
    }
    printf("{\"raw_race_round\":%u,\"successful_creators\":%u}\n",round,successes);
    EphemeralKey check;s=NCryptOpenKey(main.provider,&check.key,name.c_str(),0,0);result("raw_race_reopen_after_delete",s);
    if(s!=NTE_BAD_KEYSET)throw std::runtime_error("raw race residual");
  }
  {
    // Both handles are prepared before either is finalized. A preflight check
    // cannot protect this schedule; finalization itself must reject the loser.
    Owner winner;winner.name=main.name+L"-race";
    require("gate_race_provider",NCryptOpenStorageProvider(&winner.provider,MS_KEY_STORAGE_PROVIDER,0));
    require("gate_race_first_prepare",create_import_handle(winner.provider,original,winner.name,raw,&winner.key));
    EphemeralKey contender;
    require("gate_race_second_prepare",NCryptCreatePersistedKey(main.provider,&contender.key,reinterpret_cast<const wchar_t*>(algorithm.data()),winner.name.c_str(),0,0));
    BYTE competing_marker[32];memcpy(competing_marker,marker,32);competing_marker[0]^=1;
    pending_policy(winner.key,acl,marker);pending_policy(contender.key,acl,competing_marker);
    require("gate_race_first_finalize",NCryptFinalizeKey(winner.key,0));winner.owned_key=true;
    auto before=reopened_snapshot(main.provider,winner.name,"race_before");
    s=NCryptFinalizeKey(contender.key,0);result("gate_race_second_finalize",s);
    auto after=reopened_snapshot(main.provider,winner.name,"race_after");
    printf("{\"race_public_unchanged\":%s,\"race_all_properties_unchanged\":%s}\n",before.fields[0]==after.fields[0]?"true":"false",before==after?"true":"false");
    if(s!=NTE_EXISTS)throw std::runtime_error("candidate late collision accepted");
    if(!(before==after))throw std::runtime_error("late collision changed winner");
    result("candidate_late_collision_sentinel_unchanged",0);
  }
}
