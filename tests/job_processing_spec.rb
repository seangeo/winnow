# Copyright (c) 2007-2010 The Kaphan Foundation
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

require File.dirname(__FILE__) + "/spec_helper.rb"
require File.dirname(__FILE__) + "/test_http_server.rb"

gem 'ratom'
require 'atom'

gem 'auth-hmac'
require 'auth-hmac'

describe "Classifier Job Processing" do
  before(:each) do
    start_classifier
    @http = TestHttpServer.new(:port => 8888)
  end
  
  after(:each) do
    stop_classifier
    @http.shutdown
  end
  
  it "should attempt to fetch a tag for the URL given in the Job description" do
    @http.should_receive do
      request("/mytag-training.atom") do |req, res|
        req.request_method.should == 'GET'
      end
    end    
    
    job = create_job('http://localhost:8888/mytag-training.atom')
    @http.should have_received_requests
  end
  
  it "should not include IF-MODIFIED-SINCE on first request" do
    @http.should_receive do
      request("/mytag-training.atom") do |req, res|
        req['IF-MODIFIED-SINCE'].should be_nil
      end
    end    
    
    job = create_job('http://localhost:8888/mytag-training.atom')
    @http.should have_received_requests
  end
  
  it "should include IF-MODIFIED-SINCE on second request" do
    requests = 0
    @http.should_receive do
      request("/mytag-training.atom", 2) do |req, res|
        requests += 1
        
        if requests == 1
          res.body = File.read(File.join(File.dirname(__FILE__), 'fixtures', 'complete_tag.atom'))
        else
          req['IF-MODIFIED-SINCE'].should == "Sun, 30 Mar 2008 01:24:18 GMT" # Date in atom:updated
          res.status = 304
        end
      end
    end
    
    job = create_job('http://localhost:8888/mytag-training.atom')
    sleep(2)
    create_job('http://localhost:8888/mytag-training.atom')    
    
    @http.should have_received_requests
  end
    
  it "should update the tagger on the second request" do
    @http.should_receive do
      request("/mytag-training.atom", 2) do |req, res|
        res.body = File.read(File.join(File.dirname(__FILE__), 'fixtures', 'complete_tag.atom'))        
      end
    end
    
    job = create_job('http://localhost:8888/mytag-training.atom')
    sleep(2)
    create_job('http://localhost:8888/mytag-training.atom')    
    
    @http.should have_received_requests
  end
  
  it "should accept application/atom+xml" do
    @http.should_receive do
      request("/mytag-training.atom") do |req, res|
        req['ACCEPT'].should == "application/atom+xml"
      end
    end
    
    job = create_job('http://localhost:8888/mytag-training.atom')
    @http.should have_received_requests
  end
  
  it "should not crash if it gets a 404" do
    @http.should_receive do
      request("/mytag-training.atom") do |req, res|
        res.status = 404
      end
    end
    
    job = create_job('http://localhost:8888/mytag-training.atom')
    
    should_not_crash
    @http.should have_received_requests
  end
  
  it "should not crash if it gets a 500" do
    @http.should_receive do
      request("/mytag-training.atom") do |req, res|
        res.status = 500
      end
    end
    
    job = create_job('http://localhost:8888/mytag-training.atom')
    should_not_crash
    @http.should have_received_requests
  end
  
  it "should not crash if it gets junk back in the body" do
    @http.should_receive do
      request("/mytag-training.atom") do |req, res|
        res.body = "blahbalh"
      end
    end
    
    job = create_job('http://localhost:8888/mytag-training.atom')
    should_not_crash
    @http.should have_received_requests
  end
  
  it "should PUT results to the classifier url" do    
    job_results do |req, res|
      res.request_method.should == 'PUT'
    end
  end
  
  it "should have application/atom+xml as the content type" do
    job_results do |req, res|
      req['content-type'].should == 'application/atom+xml'
    end
  end
  
  it "should have the classifier in the user agent" do
    job_results do |req, res|
      req['user-agent'].should match(/PeerworksClassifier/)
    end
  end
  
  it "should have a valid atom document as the body" do
    job_results do |req, res|
      lambda { Atom::Feed.load_feed(req.body) }.should_not raise_error
    end
  end
  
  it "should have a tag's id as the id of the feed in the atom document" do
    job_results do |req, res|
      feed = Atom::Feed.load_feed(req.body)
      feed.id.should == "http://trunk.mindloom.org:80/seangeo/tags/a-religion"
    end
  end
  
  it "should have an entry for each item" do
    job_results do |req, res|
      feed = Atom::Feed.load_feed(req.body)
      feed.should have(10).entries
    end
  end
  
  it "should have ids for each entry that match the item id" do
    job_results do |req, res|
      feed = Atom::Feed.load_feed(req.body)
      feed.entries.each do |e|
        e.id.should match(/urn:peerworks.org:entry#/)
      end
    end
  end
  
  it "should have a single category for each entry" do
    job_results do |req, res|
      feed = Atom::Feed.load_feed(req.body)
      feed.entries.each do |e|
        e.should have(1).categories
      end
    end
  end
  
  it "should have a strength attribute on each category" do
    job_results do |req, res|
      Atom::Feed.load_feed(req.body).entries.each do |e|
        e.categories.first['http://peerworks.org/classifier', 'strength'].should_not be_empty
        e.categories.first['http://peerworks.org/classifier', 'strength'].first.should match(/\d+\.\d+/)
      end
    end    
  end
    
  it "should have a term and scheme that match the term and scheme on the tag definition" do
    job_results do |req, res|
      feed = Atom::Feed.load_feed(req.body)
      feed.entries.each do |e|
        e.categories.first.term.should == 'a-religion'
        e.categories.first.scheme.should == 'http://trunk.mindloom.org:80/seangeo/tags/'
      end
    end
  end
  
  it "should include a new classified date on the tag" do
    job_results do |req, res|
      Atom::Feed.load_feed(req.body)['http://peerworks.org/classifier', 'classified'].should_not be_empty
      new_time = Time.parse(Atom::Feed.load_feed(req.body)['http://peerworks.org/classifier', 'classified'].first)
      new_time.should > Time.parse('2008-04-15T01:16:23Z')
    end
  end
end

describe "Job Processing with a threshold set" do
  before(:each) do
    start_classifier(:positive_threshold => 0.9)
    @http = TestHttpServer.new(:port => 8888)
  end
  
  after(:each) do
    stop_classifier
    @http.shutdown
  end
  
  it "should only send entries for items above the threshold" do
    job_results do |req, res|
      Atom::Feed.load_feed(req.body).should have(4).entries
    end
  end
end
  
describe "Job Processing with an incomplete tag" do
  before(:each) do
    start_classifier(:positive_threshold => 0.9)
    @http = TestHttpServer.new(:port => 8888)
  end
  
  after(:each) do
    stop_classifier
    @http.shutdown
  end
  
  it "should result in new items in the item cache" do
    job_results('incomplete_tag.atom')
    sqlite = SQLite3::Database.open("#{Database}/catalog.db")
    sqlite.get_first_value("select count(*) from entries").should == '13'
    sqlite.close
  end
  
  it "should PUT results" do
    job_results('incomplete_tag.atom') do |req, res|
      req.request_method.should == 'PUT'
    end
  end
end

describe "Job Processing after item addition" do
  before(:each) do
    @item_count = 10    
    @http = TestHttpServer.new(:port => 8888)
    start_classifier(:tag_index => 'http://localhost:8888/tags.atom')
  end

  after(:each) do
    stop_classifier
  end

  it "should include the added item" do
    create_entry
    sleep(1)
    job_results do |req, res|
      Atom::Feed.load_feed(req.body).should have(@item_count + 1).entries
    end
  end

  it "should automatically classify the new item" do
    @http.should_receive do
      request("/tags.atom") do |req, res|
        res.body = File.read(File.dirname(__FILE__) + "/fixtures/tag_index.atom")
      end
      
      request("/quentin/tags/tag/training.atom") do |req, res|
        res.body = File.read(File.dirname(__FILE__) + "/fixtures/complete_tag.atom")
      end
      
      request("/results") do |req, res|
        req.request_method.should == "POST"
        req['content-type'].should == 'application/atom+xml'
        Atom::Feed.load_feed(req.body).should have(1).entries
      end
    end

    create_entry
    @http.should have_received_requests
  end

  it "should only create a single job that classifies both items for each tag" do
    @http.should_receive do
      request("/tags.atom") do |req, res|
        res.body = File.read(File.dirname(__FILE__) + "/fixtures/tag_index.atom")
      end
      
      request("/quentin/tags/tag/training.atom") do |req, res|
        res.body = File.read(File.dirname(__FILE__) + "/fixtures/complete_tag.atom")
      end
      
      request("/results") do |req, res|
        req.request_method.should == "POST"
        req['content-type'].should == 'application/atom+xml'
        Atom::Feed.load_feed(req.body).should have(2).entries
      end
    end
    
    create_entry(:id => "1111")
    create_entry(:id => "1112")
    @http.should have_received_requests
    #`wc -l /tmp/perf.log`.to_i.should == Tagging.count(:select => 'distinct tag_id') + 2 # +1 for header, +1 for previous job
  end
end

describe 'Job Processing using files' do
  before(:each) do
    FileUtils.rm_f('/tmp/taggings.atom')
    start_classifier()
  end
  
  after(:each) do
    stop_classifier
  end
  
  it "should fetch the tag file and save the results" do
    times = 0
    job = create_job("file:#{File.expand_path(File.dirname(__FILE__) + "/fixtures/file_tag.atom")}")
    while job.progress < 100
      job.reload
      sleep(0.1)
      times += 1
      times.should < 10
    end
    File.exist?("/tmp/taggings.atom").should be_true
  end
end

describe 'Job Processing with authentication' do
  before(:each) do
    start_classifier(:credentials => File.join(ROOT, 'fixtures', 'credentials.js'))
    @http = TestHttpServer.new(:port => 8888)
    @authhmac = AuthHMAC.new('classifier_id' => 'classifier_secret')
  end
  
  after(:each) do
    stop_classifier
    @http.shutdown
  end
  
  it "should send an authorization header when getting the tag" do
    @http.should_receive do
      request("/mytag-training.atom") do |req, res|
        AuthHMAC.new('classifier_id' => 'classifier_secret').authenticated?(req).should be_true
      end
    end
    
    job = create_job('http://localhost:8888/mytag-training.atom')
    @http.should have_received_requests
  end
  
  it "should send correct authorization header when getting the tag with a space in the name" do
    @http.should_receive do
      request("/mytag training.atom") do |req, res|
        AuthHMAC.new('classifier_id' => 'classifier_secret').authenticated?(req).should be_true
      end
    end
    
    job = create_job('http://localhost:8888/mytag%20training.atom')
    @http.should have_received_requests
  end
  
  it "should send the Authorization header with the results" do
    job_results do |req, res|
      @authhmac.authenticated?(req).should be_true
    end    
  end
end
    
def job_results(atom = 'complete_tag.atom', times_gotten = 1)
  gets = 0
  @http.should_receive do
    request("/mytag-training.atom", times_gotten) do |req, res|
      gets += 1
      
      if gets > 1
        res.status = '304'
      else
        res.body = File.read(File.join(File.dirname(__FILE__), 'fixtures', atom))
      end
    end
    request("/results") do |req, res|
      yield(req, res) if block_given?
    end
  end
  job = create_job('http://localhost:8888/mytag-training.atom')
  
  @http.should have_received_requests
end

def should_not_crash
  lambda {create_job('http://localhost:8888/mytag-training.atom')}.should_not raise_error
end
