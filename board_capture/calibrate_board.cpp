#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <numeric>
#include <array>

using namespace cv;
using namespace std;

// -------------------- 手動クリック用 --------------------
struct ClickState {
    vector<Point2f> pts;
    bool done=false;
} g_click;

static void on_mouse(int event, int x, int y, int, void* userdata){
    if(event==EVENT_LBUTTONDOWN){
        auto* st = reinterpret_cast<ClickState*>(userdata);
        if(st->pts.size()<4){
            st->pts.push_back(Point2f((float)x,(float)y));
            if(st->pts.size()==4) st->done=true;
        }
    }
}

// -------------------- 1回だけHを決めてロック --------------------
// 成功: true（H_outに格納）/ 失敗: false
static bool get_homography_once(const Mat& frameBGR, Size outSize, Mat& H_out, bool forceManual=false){
    auto makeH = [&](const Point2f src[4])->Mat{
        Point2f dst[4] = {
            {0,0},
            {(float)outSize.width,0},
            {(float)outSize.width,(float)outSize.height},
            {0,(float)outSize.height}
        };
        return getPerspectiveTransform(src, dst);
    };

    if(!forceManual){
        // 自動検出（黄/緑の大きな四角形を拾う）
        Mat hsv; cvtColor(frameBGR, hsv, COLOR_BGR2HSV);
        Mat maskY, maskG;
        inRange(hsv, Scalar(15,60,60), Scalar(40,255,255), maskY);
        inRange(hsv, Scalar(45,60,40), Scalar(85,255,255), maskG);
        Mat mask = maskY | maskG;

        vector<vector<Point>> cs;
        findContours(mask, cs, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
        vector<Point> approx;
        if(!cs.empty()){
            auto it = max_element(cs.begin(), cs.end(),
                                  [](auto& a, auto& b){ return contourArea(a) < contourArea(b); });
            approxPolyDP(*it, approx, 0.02*arcLength(*it,true), true);
        }
        if(approx.size()==4){
            sort(approx.begin(), approx.end(), [](const Point& p1, const Point& p2){
                return (p1.y<p2.y) || (p1.y==p2.y && p1.x<p2.x);
            });
            Point2f tl = approx[0].x < approx[1].x ? approx[0] : approx[1];
            Point2f tr = approx[0].x < approx[1].x ? approx[1] : approx[0];
            Point2f bl = approx[2].x < approx[3].x ? approx[2] : approx[3];
            Point2f br = approx[2].x < approx[3].x ? approx[3] : approx[2];
            Point2f src[4] = {tl,tr,br,bl};
            H_out = makeH(src);
            return true;
        }
    }

    // 手動クリック（TL→TR→BR→BL）
    ClickState st; g_click = st;
    Mat show = frameBGR.clone();
    putText(show, "Click TL, TR, BR, BL (ESC=cancel)", {20,30},
            FONT_HERSHEY_SIMPLEX, 0.8, Scalar(0,0,255), 2);
    namedWindow("click", WINDOW_NORMAL);
    setMouseCallback("click", on_mouse, &g_click);

    while(true){
        Mat tmp = frameBGR.clone();
        for(size_t i=0;i<g_click.pts.size();++i){
            circle(tmp, g_click.pts[i], 6, Scalar(0,0,255), FILLED);
            putText(tmp, to_string(i), g_click.pts[i]+Point2f(5,-5),
                    FONT_HERSHEY_PLAIN, 1.2, Scalar(0,0,255), 2);
        }
        imshow("click", tmp);
        int key = waitKey(30);
        if(g_click.done) break;
        if(key==27){ destroyWindow("click"); return false; } // ESC で中止
    }
    destroyWindow("click");
    if(g_click.pts.size()!=4) return false;

    Point2f src[4] = { g_click.pts[0], g_click.pts[1], g_click.pts[2], g_click.pts[3] };
    H_out = makeH(src);
    return true;
}

// -------------------- ユーティリティ --------------------
static Mat warp_with_H(const Mat& frame, const Mat& H, Size outSize){
    Mat board;
    warpPerspective(frame, board, H, outSize);
    return board;
}

static double edge_density(const Mat& gray){
    Mat gx, gy; Sobel(gray, gx, CV_32F, 1, 0);
    Sobel(gray, gy, CV_32F, 0, 1);
    Mat mag; magnitude(gx, gy, mag);
    return mean(mag)[0];
}

// -------------------- 統計（空盤キャリブ用） --------------------
struct Stats {
    array<double,64> Lmean{}, Amean{}, Bmean{}, Emean{};
    array<double,64> Lm2{},   Am2{},   Bm2{},   Em2{};
    array<int,   64> n{};

    void accumulate(const Mat& boardBGR){
        Mat lab; cvtColor(boardBGR, lab, COLOR_BGR2Lab);
        int cellW = boardBGR.cols/8, cellH = boardBGR.rows/8;
        int idx=0;
        for(int y=0;y<8;++y){
            for(int x=0;x<8;++x,++idx){
                Rect r(x*cellW, y*cellH, cellW, cellH);
                Mat roiBGR = boardBGR(r);
                Mat roiLAB = lab(r);
                vector<Mat> ch; split(roiLAB, ch);
                double L = mean(ch[0])[0];
                double A = mean(ch[1])[0];
                double B = mean(ch[2])[0];

                Mat gray; cvtColor(roiBGR, gray, COLOR_BGR2GRAY);
                double E = edge_density(gray);

                auto upd = [&](double val, double& mean, double& m2, int& count){
                    count++;
                    double delta = val - mean;
                    mean += delta / count;
                    double delta2 = val - mean;
                    m2 += delta * delta2;
                };
                upd(L, Lmean[idx], Lm2[idx], n[idx]);
                upd(A, Amean[idx], Am2[idx], n[idx]);
                upd(B, Bmean[idx], Bm2[idx], n[idx]);
                upd(E, Emean[idx], Em2[idx], n[idx]);
            }
        }
    }

    void exportYaml(const string& path, double Te, double Tc, const Mat& H){
        FileStorage fs(path, FileStorage::WRITE);
        fs << "cells" << 64;
        fs << "L_mean" << Mat(8,8,CV_64F, (void*)Lmean.data());
        fs << "A_mean" << Mat(8,8,CV_64F, (void*)Amean.data());
        fs << "B_mean" << Mat(8,8,CV_64F, (void*)Bmean.data());
        fs << "E_mean" << Mat(8,8,CV_64F, (void*)Emean.data());

        auto m2_to_std = [&](const array<double,64>& m2, const array<int,64>& nn){
            Mat out(8,8,CV_64F);
            for(int i=0;i<64;++i){
                int cnt = nn[i];
                double var = (cnt>1)? (m2[i]/(cnt-1)) : 0.0;
                out.at<double>(i/8, i%8) = sqrt(max(0.0, var));
            }
            return out;
        };
        fs << "L_std" << m2_to_std(Lm2, n);
        fs << "A_std" << m2_to_std(Am2, n);
        fs << "B_std" << m2_to_std(Bm2, n);
        fs << "E_std" << m2_to_std(Em2, n);

        fs << "Te_edge" << Te;
        fs << "Tc_Ldiff" << Tc;

        if(!H.empty()){
            fs << "H" << H; // 3x3 ホモグラフィも保存
        }
        fs.release();
    }
};

static double mean_all(const array<double,64>& v){
    double s=0; for(double x: v) s+=x; return s/64.0;
}
static double std_all(const array<double,64>& mean, const array<double,64>& m2, const array<int,64>& n){
    double s=0; int c=0;
    for(int i=0;i<64;++i){
        if(n[i]>1){
            s += (m2[i]/(n[i]-1));
            c++;
        }
    }
    double var = (c>0)? s / c : 0.0;
    return sqrt(max(0.0, var));
}

// -------------------- メイン --------------------
int main(int argc, char** argv){
    int cam=0, frames=60;
    string outPath="calib.yaml", verifyPath="", devPath="";
    bool manual=false;

    for(int i=1;i<argc;++i){
        string a=argv[i];
        if(a=="--camera" && i+1<argc) cam=atoi(argv[++i]);
        else if(a=="--device" && i+1<argc) devPath=argv[++i];
        else if(a=="--frames" && i+1<argc) frames=atoi(argv[++i]);
        else if(a=="--out" && i+1<argc) outPath=argv[++i];
        else if(a=="--verify" && i+1<argc) verifyPath=argv[++i];
        else if(a=="--manual") manual=true; // 常に手動クリックしたい時
    }

    // -------------------- 検証モード --------------------
    if(!verifyPath.empty()){
        FileStorage fs(verifyPath, FileStorage::READ);
        if(!fs.isOpened()){ cerr<<"cannot open "<<verifyPath<<"\n"; return 1; }
        Mat Lmean,Amean,Bmean,Emean,Lstd,Astd,Bstd,Estd,H;
        double Te,Tc;
        fs["L_mean"]>>Lmean; fs["A_mean"]>>Amean; fs["B_mean"]>>Bmean; fs["E_mean"]>>Emean;
        fs["L_std"]>>Lstd;   fs["A_std"]>>Astd;   fs["B_std"]>>Bstd;   fs["E_std"]>>Estd;
        fs["Te_edge"]>>Te;   fs["Tc_Ldiff"]>>Tc;
        fs["H"]>>H; // あれば読む
        fs.release();

        cout<<"Loaded calib. Te="<<Te<<" Tc="<<Tc<<"\n";

        VideoCapture cap;
        if(!devPath.empty()) cap.open(devPath, cv::CAP_V4L2);
        else                 cap.open(cam,    cv::CAP_V4L2);
        if(!cap.isOpened()){ cerr<<"camera open failed\n"; return 1; }

        // 解像度指定（必要に応じて）
        cap.set(CAP_PROP_FOURCC, VideoWriter::fourcc('M','J','P','G'));
        cap.set(CAP_PROP_FRAME_WIDTH, 1920);
        cap.set(CAP_PROP_FRAME_HEIGHT,1080);
        cap.set(CAP_PROP_FPS, 30);

        Size outSize(800,800);

        // H が無い場合は、最初のフレームから1回だけ決める
        if(H.empty()){
            Mat first; cap>>first; if(first.empty()){ cerr<<"no frame\n"; return 1; }
            if(!get_homography_once(first, outSize, H, manual)){
                cerr<<"homography selection canceled or failed\n"; return 1;
            }
        }

        int frame_count = 0;
        bool auto_tracking = true; // 自動追跡をデフォルトON
        
        while(true){
            Mat f; cap>>f; if(f.empty()) break;
            
            // 自動追跡が有効な場合のみ更新を試みる
            if(auto_tracking){
                Mat hsv; cvtColor(f, hsv, COLOR_BGR2HSV);
                Mat maskY, maskG;
                inRange(hsv, Scalar(20,80,80), Scalar(35,255,255), maskY);
                inRange(hsv, Scalar(40,50,50), Scalar(80,255,255), maskG);
                Mat mask = maskY | maskG;
                
                // ノイズ除去
                Mat kernel = getStructuringElement(MORPH_RECT, Size(5,5));
                morphologyEx(mask, mask, MORPH_CLOSE, kernel);
                morphologyEx(mask, mask, MORPH_OPEN, kernel);
                
                vector<vector<Point>> cs;
                findContours(mask, cs, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
                
                // 面積でフィルタリング（画面の5%以上）
                double minArea = f.rows * f.cols * 0.05;
                vector<Point> approx;
                double maxArea = 0;
                
                for(const auto& contour : cs){
                    double area = contourArea(contour);
                    if(area > minArea && area > maxArea){
                        vector<Point> temp;
                        approxPolyDP(contour, temp, 0.02*arcLength(contour,true), true);
                        if(temp.size()==4){
                            approx = temp;
                            maxArea = area;
                        }
                    }
                }
                
                if(approx.size()==4){
                    sort(approx.begin(), approx.end(), [](const Point& p1, const Point& p2){
                        return (p1.y<p2.y) || (p1.y==p2.y && p1.x<p2.x);
                    });
                    Point2f tl = approx[0].x < approx[1].x ? (Point2f)approx[0] : (Point2f)approx[1];
                    Point2f tr = approx[0].x < approx[1].x ? (Point2f)approx[1] : (Point2f)approx[0];
                    Point2f bl = approx[2].x < approx[3].x ? (Point2f)approx[2] : (Point2f)approx[3];
                    Point2f br = approx[2].x < approx[3].x ? (Point2f)approx[3] : (Point2f)approx[2];
                    Point2f src[4] = {tl,tr,br,bl};
                    Point2f dst[4] = {
                        {0,0},
                        {(float)outSize.width,0},
                        {(float)outSize.width,(float)outSize.height},
                        {0,(float)outSize.height}
                    };
                    H = getPerspectiveTransform(src, dst);
                }
            }
            
            // キー操作
            int key = waitKey(1);
            if(key == 'r' || key == 'R'){
                cout<<"Manual recalculation...\n";
                if(get_homography_once(f, outSize, H, true)){
                    cout<<"Homography updated!\n";
                }
            }
            if(key == 'a' || key == 'A'){
                auto_tracking = !auto_tracking;
                cout<<"Auto-tracking: "<<(auto_tracking?"ON":"OFF")<<"\n";
            }
            if(key == 27) break; // ESC
            
            Mat board = warp_with_H(f, H, outSize);

            // 占有スコアの可視化（赤=occupied, 緑=empty）
            Mat lab; cvtColor(board, lab, COLOR_BGR2Lab);
            int cw=100, ch=100;
            for(int y=0;y<8;++y){
                for(int x=0;x<8;++x){
                    Rect r(x*cw,y*ch,cw,ch);
                    Mat roiBGR=board(r), roiLAB=lab(r);
                    vector<Mat> chs; split(roiLAB, chs);
                    double L=mean(chs[0])[0];
                    Mat gray; cvtColor(roiBGR, gray, COLOR_BGR2GRAY);
                    double E = edge_density(gray);

                    double L0 = Lmean.at<double>(y,x);
                    double e0 = Emean.at<double>(y,x);
                    double e_score = (E - e0) / (Te + 1e-6);
                    double l_score = abs(L - L0) / (Tc + 1e-6);
                    bool occ = (e_score > 1.0 && l_score > 0.5) || (e_score > 2.0);
                    Scalar col = occ? Scalar(0,0,255):Scalar(0,255,0);
                    rectangle(board, r, col, 2);
                }
            }
            putText(board, "Press 'R' to recalibrate position, ESC to quit", {10,780},
                    FONT_HERSHEY_SIMPLEX, 0.6, Scalar(255,255,255), 1);
            imshow("board", board);
        }
        return 0;
    }

    // -------------------- キャリブレーション実行 --------------------
    VideoCapture cap;
    if(!devPath.empty()) cap.open(devPath, cv::CAP_V4L2);
    else                 cap.open(cam,    cv::CAP_V4L2);
    if(!cap.isOpened()){ cerr<<"camera open failed\n"; return 1; }

    // 解像度/フォーマット（必要に応じて）
    cap.set(CAP_PROP_FOURCC, VideoWriter::fourcc('M','J','P','G'));
    cap.set(CAP_PROP_FRAME_WIDTH, 1920);
    cap.set(CAP_PROP_FRAME_HEIGHT,1080);
    cap.set(CAP_PROP_FPS, 30);

    // まず1フレームだけ取り、Hを決めてロック
    Mat first;
    cap >> first;
    if(first.empty()){ cerr<<"no frame\n"; return 1; }

    Size outSize(800,800);
    Mat H;
    if(!get_homography_once(first, outSize, H, manual)){
        cerr<<"homography selection canceled or failed\n"; return 1;
    }
    cout<<"Homography locked. Capturing "<<frames<<" empty-board frames...\n";

    Stats st;
    int captured=0;
    while(captured<frames){
        Mat f; cap>>f; if(f.empty()) break;
        Mat board = warp_with_H(f, H, outSize);
        st.accumulate(board);

        Mat show=board.clone();
        putText(show, "Calibrating: "+to_string(captured+1)+"/"+to_string(frames),
                {20,40}, FONT_HERSHEY_SIMPLEX, 1.0, Scalar(0,0,255), 2);
        imshow("calib", show);
        if(waitKey(1)==27) return 0;
        captured++;
    }

    double E_sigma = std_all(st.Emean, st.Em2, st.n);
    double L_sigma = std_all(st.Lmean, st.Lm2, st.n);
    double k=4.0;
    double Te = k*E_sigma;  // e - e0 > Te
    double Tc = k*L_sigma;  // |L - L0| > Tc

    st.exportYaml(outPath, Te, Tc, H);
    cout<<"Saved "<<outPath<<"\n";
    cout<<"Te(edge)="<<Te<<"  Tc(Ldiff)="<<Tc<<"\n";
    cout<<"Done.\n";
    return 0;
}
