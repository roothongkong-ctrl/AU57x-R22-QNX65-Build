import Foundation

// Only skip after a previously successful start. Never violate stop-after-current,
// single-track, or stop-at-end policies. Bound recovery to distinct queue entries.
enum RecoveryPolicy {
    static func candidates(count:Int,index:Int,failed:Set<Int>,mode:String,single:Bool,sleepMinutes:Int)->[Int] {
        guard count>1,index>=0,index<count,!single,sleepMinutes != -1,mode != "single" || sleepMinutes == -2 else{return []}
        let sequence:[Int]
        if sleepMinutes == -2 { sequence=Array((index+1)..<count) }
        else { sequence=(1..<count).map{(index+$0)%count} }
        let remaining=sequence.filter{!failed.contains($0)}
        return mode=="random" && sleepMinutes != -2 ? remaining.shuffled():remaining
    }
}
