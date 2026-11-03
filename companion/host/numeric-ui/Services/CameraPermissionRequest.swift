import Foundation
import CoreFoundation

public enum CameraPermissionMode:String { case webcam,gestures }
public enum CameraPermissionRequestFailure:Error { case explicitIntentRequired,malformed }

/// Existing-app local worker request serialization only. No issuer, trust bool,
/// permission grant, camera, socket, model, or capture operation is constructed.
public struct CameraPermissionRequest {
    public let requestID:String
    public let mode:CameraPermissionMode
    public let ttlSeconds:Int
    private let opaqueAuthority:String
    public init(requestID:String,mode:CameraPermissionMode,ttlSeconds:Int,opaqueAuthority:String,explicitLocalIntent:Bool)throws {
        guard explicitLocalIntent else {throw CameraPermissionRequestFailure.explicitIntentRequired}
        guard !requestID.isEmpty,requestID.utf8.count<=64,requestID.utf8.allSatisfy({(48...57).contains($0) || (65...90).contains($0) || (97...122).contains($0) || [45,46,58,95].contains($0)}),
              (1...60).contains(ttlSeconds),!opaqueAuthority.isEmpty,opaqueAuthority.utf8.count<=1024 else {throw CameraPermissionRequestFailure.malformed}
        self.requestID=requestID;self.mode=mode;self.ttlSeconds=ttlSeconds;self.opaqueAuthority=opaqueAuthority
    }
    public func localWorkerJSON()throws->Data {
        let value:[String:Any]=["type":"camera_permission_request","version":1,"request_id":requestID,
            "mode":mode.rawValue,"ttl_seconds":ttlSeconds,"opaque_authority":opaqueAuthority]
        let data=try JSONSerialization.data(withJSONObject:value,options:[.sortedKeys])
        guard data.count<=2048 else {throw CameraPermissionRequestFailure.malformed};return data
    }
    public static func explicitOff(requestID:String)throws->Data {
        guard !requestID.isEmpty,requestID.utf8.count<=64,requestID.utf8.allSatisfy({(48...57).contains($0) || (65...90).contains($0) || (97...122).contains($0) || [45,46,58,95].contains($0)}) else {throw CameraPermissionRequestFailure.malformed}
        return try JSONSerialization.data(withJSONObject:["type":"camera_permission_off","version":1,"request_id":requestID],options:[.sortedKeys])
    }
}

/// Local worker feedback only. This notice cannot create authority or readiness.
/// Capture/enable flags must stay false in this source-only candidate.
public struct CameraPermissionNotice {
    public let requestID:String
    public let feedback:String
    public static func decode(_ data:Data)->CameraPermissionNotice? {
        guard data.count<=2048,
              let value=(try? JSONSerialization.jsonObject(with:data)) as? [String:Any],
              Set(value.keys)==Set(["type","version","request_id","permission_current","clock_ready","capture_ready","enabled","reason"]),
              value["type"] as? String == "camera_permission_status",
              let version=value["version"] as? NSNumber,CFGetTypeID(version) != CFBooleanGetTypeID(),version.doubleValue==1,
              let request=value["request_id"] as? String,!request.isEmpty,request.utf8.count<=64,
              request.utf8.allSatisfy({(48...57).contains($0) || (65...90).contains($0) || (97...122).contains($0) || [45,46,58,95].contains($0)}),
              let capture=value["capture_ready"] as? NSNumber,CFGetTypeID(capture)==CFBooleanGetTypeID(),!capture.boolValue,
              let enabled=value["enabled"] as? NSNumber,CFGetTypeID(enabled)==CFBooleanGetTypeID(),!enabled.boolValue,
              let permission=value["permission_current"] as? NSNumber,CFGetTypeID(permission)==CFBooleanGetTypeID(),
              let clock=value["clock_ready"] as? NSNumber,CFGetTypeID(clock)==CFBooleanGetTypeID(),
              let reason=value["reason"] as? String,reason.utf8.count<=256 else {return nil}
        return CameraPermissionNotice(requestID:request,feedback:permission.boolValue ?
            "Camera permission acknowledged · capture unavailable until qualified" : "Camera permission unavailable · "+reason)
    }
}

/// A fresh explicit Mac choice; the current device must separately confirm it.
/// No opaque grant is supplied by UI and capture readiness remains separate.
public struct CameraPermissionProposal {
    public let requestID:String
    public let mode:CameraPermissionMode
    public let ttlSeconds:Int
    public init(requestID:String,mode:CameraPermissionMode,ttlSeconds:Int)throws {
        guard !requestID.isEmpty,requestID.utf8.count<=64,requestID.utf8.allSatisfy({(48...57).contains($0) || (65...90).contains($0) || (97...122).contains($0) || [45,46,58,95].contains($0)}),(1...60).contains(ttlSeconds) else {throw CameraPermissionRequestFailure.malformed}
        self.requestID=requestID;self.mode=mode;self.ttlSeconds=ttlSeconds
    }
    public func localWorkerJSON()throws->Data {
        try JSONSerialization.data(withJSONObject:["type":"camera_permission_prepare","version":1,"request_id":requestID,"mode":mode.rawValue,"ttl_seconds":ttlSeconds],options:[.sortedKeys])
    }
}
